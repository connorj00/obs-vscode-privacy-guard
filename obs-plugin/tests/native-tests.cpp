#include "privacy-state.hpp"
#include "protocol.hpp"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

	using privacy_guard::FrameKind;
	using privacy_guard::Mode;
	using privacy_guard::PrivacyState;
	using namespace std::chrono_literals;

	constexpr char client_one[] = "01234567-89ab-cdef-0123-456789abcdef";
	constexpr char client_two[] = "fedcba98-7654-3210-fedc-ba9876543210";

	void require(const bool condition, const std::string &message) {
		if (!condition) {
			throw std::runtime_error(message);
		}
	}

	void test_protocol_parser() {
		const auto hello = privacy_guard::parse_frame(std::string("PG/1 HELLO ") + client_one);
		require(hello.has_value() && hello->kind == FrameKind::Hello, "valid HELLO should parse");
		require(hello->client_id == client_one, "HELLO should retain client id");

		const auto safe = privacy_guard::parse_frame("PG/1 STATE 1 SAFE");
		require(safe.has_value() && safe->kind == FrameKind::State && !safe->sensitive && safe->sequence == 1, "valid SAFE state should parse");

		const auto sensitive = privacy_guard::parse_frame("PG/1 STATE 2 SENSITIVE");
		require(sensitive.has_value() && sensitive->sensitive, "valid SENSITIVE state should parse");

		const auto goodbye = privacy_guard::parse_frame("PG/1 GOODBYE");
		require(goodbye.has_value() && goodbye->kind == FrameKind::Goodbye, "valid goodbye should parse");

		require(!privacy_guard::parse_frame("PG/1  GOODBYE").has_value(), "repeated separators should be rejected");
		require(!privacy_guard::parse_frame("PG/1 GOODBYE ").has_value(), "trailing separators should be rejected");
		require(!privacy_guard::parse_frame("PG/1 STATE x SAFE").has_value(), "non-numeric sequences should be rejected");
		require(!privacy_guard::parse_frame("PG/1 HEARTBEAT 3").has_value(), "unused heartbeat frames should be rejected");
		require(!privacy_guard::parse_frame("PG/1 HELLO C:/secret.env").has_value(), "non-UUID identifiers should be rejected");
		require(!privacy_guard::parse_frame("PG/2 GOODBYE").has_value(), "unknown protocol versions should be rejected");
	}

	void test_disconnect_recovery_requires_a_new_client() {
		PrivacyState state(2s);
		const auto start = PrivacyState::Clock::now();
		const auto first = state.connection_opened(start);
		const auto second = state.connection_opened(start);

		require(state.hello_received(first, client_one, start), "first client HELLO should be accepted");
		require(state.state_received(first, 1, false, start), "first client SAFE should be accepted");
		require(state.hello_received(second, client_two, start), "second client HELLO should be accepted");
		require(state.state_received(second, 1, false, start), "second client SAFE should be accepted");

		state.connection_closed(second);
		require(state.mode() == Mode::Disconnected, "a failed client must latch the aggregate state");
		require(state.state_received(first, 2, false, start + 500ms), "the remaining client's next state should be accepted");
		require(state.mode() == Mode::Disconnected, "an existing client must not recover a different failed client");

		const auto replacement = state.connection_opened(start + 600ms);
		require(state.hello_received(replacement, client_two, start + 600ms), "replacement client HELLO should be accepted");
		require(state.state_received(replacement, 2, false, start + 600ms), "replacement client state should be accepted");
		require(state.mode() == Mode::Safe, "a fresh replacement state should clear one failed-client latch");
	}

	void test_overlapping_reconnect_recovers() {
		PrivacyState state(2s);
		const auto start = PrivacyState::Clock::now();
		const auto original = state.connection_opened(start);
		require(state.hello_received(original, client_one, start), "original HELLO should be accepted");
		require(state.state_received(original, 1, false, start), "original SAFE should be accepted");

		const auto overlapping = state.connection_opened(start + 100ms);
		require(!state.hello_received(overlapping, client_one, start + 100ms), "an overlapping client identity must be rejected");
		state.protocol_error(overlapping);
		require(state.mode() == Mode::Disconnected, "a rejected connection must fail closed");
		require(state.state_received(original, 2, false, start + 500ms), "the established client should remain valid");
		require(state.mode() == Mode::Safe, "a valid state should clear an untrusted-connection failure");
	}

	void test_multi_client_aggregation_and_recovery() {
		PrivacyState state(2s);
		const auto start = PrivacyState::Clock::now();

		require(state.mode() == Mode::AwaitingConnection, "startup must fail closed");

		const auto first = state.connection_opened(start);
		require(state.hello_received(first, client_one, start), "first client HELLO should be accepted");
		require(state.state_received(first, 1, false, start), "first client SAFE should be accepted");
		require(state.mode() == Mode::Safe, "one healthy safe client should show output");

		const auto second = state.connection_opened(start);
		require(state.mode() == Mode::AwaitingConnection, "a pending second handshake must fail closed");
		require(state.hello_received(second, client_two, start), "second client HELLO should be accepted");
		require(state.state_received(second, 1, true, start), "second client SENSITIVE should be accepted");
		require(state.mode() == Mode::Sensitive, "any sensitive client must hide output");

		require(state.goodbye_received(second), "graceful second client close should be accepted");
		require(state.mode() == Mode::Safe, "remaining safe client should restore output");

		state.connection_closed(first);
		require(state.mode() == Mode::Disconnected, "unexpected disconnect must latch fail-closed state");

		const auto replacement = state.connection_opened(start + 100ms);
		require(state.hello_received(replacement, client_two, start + 100ms), "replacement HELLO should be accepted");
		require(state.mode() == Mode::Disconnected, "HELLO alone must not clear disconnect latch");
		require(state.state_received(replacement, 2, false, start + 100ms), "fresh replacement state should be accepted");
		require(state.mode() == Mode::Safe, "fresh explicit SAFE should recover from disconnect");
	}

	void test_sequences_and_timeouts() {
		PrivacyState state(2s);
		const auto start = PrivacyState::Clock::now();
		const auto connection = state.connection_opened(start);

		require(state.hello_received(connection, client_one, start), "HELLO should be accepted");
		require(state.state_received(connection, 10, false, start), "initial state sequence should be accepted");
		require(!state.state_received(connection, 10, false, start + 500ms), "replayed sequence must be rejected");

		state.protocol_error(connection);
		require(state.mode() == Mode::Disconnected, "protocol error must fail closed");

		const auto replacement = state.connection_opened(start + 1s);
		require(state.hello_received(replacement, client_two, start + 1s), "replacement HELLO should be accepted");
		require(state.state_received(replacement, 1, false, start + 1s), "replacement SAFE should recover");
		state.tick(start + 3s + 1ms);
		require(state.mode() == Mode::Disconnected, "expired state updates must fail closed");
	}

	void test_explicit_disable() {
		PrivacyState state(2s);
		const auto start = PrivacyState::Clock::now();
		const auto connection = state.connection_opened(start);
		require(state.hello_received(connection, client_one, start), "HELLO should be accepted");
		require(state.state_received(connection, 1, false, start), "SAFE should be accepted");
		state.set_enabled(true);
		require(state.mode() == Mode::Safe, "setting the current enabled value must not re-arm the state machine");

		state.set_enabled(false);
		require(!state.enabled() && state.mode() == Mode::Disabled, "explicit OBS-side disable should show output");
		state.set_enabled(true);
		require(state.enabled() && state.mode() == Mode::AwaitingConnection, "re-arming must require a fresh state");
	}

} // namespace

int main() {
	try {
		test_protocol_parser();
		test_multi_client_aggregation_and_recovery();
		test_disconnect_recovery_requires_a_new_client();
		test_overlapping_reconnect_recovers();
		test_sequences_and_timeouts();
		test_explicit_disable();
		std::cout << "All native privacy guard tests passed.\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << "Native test failure: " << error.what() << '\n';
		return 1;
	}
}
