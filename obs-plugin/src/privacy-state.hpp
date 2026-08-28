#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace privacy_guard {

	// Effective protection state consumed by the OBS display controller.
	enum class Mode {
		Disabled,
		AwaitingConnection,
		Safe,
		Sensitive,
		Disconnected,
	};

	// Thread-safe, fail-closed aggregation of every connected VS Code client.
	class PrivacyState final {
	  public:
		using Clock = std::chrono::steady_clock;
		using ConnectionId = std::uint64_t;

		explicit PrivacyState(std::chrono::milliseconds liveness_timeout);

		void set_enabled(bool enabled);
		[[nodiscard]] bool enabled() const;

		[[nodiscard]] ConnectionId connection_opened(Clock::time_point now);
		[[nodiscard]] bool hello_received(ConnectionId connection_id, std::string client_id, Clock::time_point now);
		[[nodiscard]] bool state_received(ConnectionId connection_id, std::uint64_t sequence, bool sensitive, Clock::time_point now);
		[[nodiscard]] bool goodbye_received(ConnectionId connection_id);

		void protocol_error(ConnectionId connection_id);
		void connection_closed(ConnectionId connection_id);
		void tick(Clock::time_point now);

		[[nodiscard]] Mode mode() const;

	  private:
		// Protocol state tracked independently for each pipe connection.
		struct Client final {
			std::string client_id;
			std::optional<std::uint64_t> last_sequence;
			std::optional<bool> sensitive;
			Clock::time_point last_update;
			bool hello_received{false};
		};

		[[nodiscard]] bool accept_sequence(Client &client, std::uint64_t sequence);
		void record_connection_failure(const Client &client);
		[[nodiscard]] Mode mode_locked() const;

		mutable std::mutex mutex_;
		std::unordered_map<ConnectionId, Client> clients_;
		std::size_t disconnected_clients_{0};
		std::chrono::milliseconds liveness_timeout_;
		ConnectionId next_connection_id_{1};
		bool enabled_{true};
		bool untrusted_failure_latched_{false};
	};

} // namespace privacy_guard
