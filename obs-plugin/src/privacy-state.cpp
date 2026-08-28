#include "privacy-state.hpp"

#include <utility>

namespace privacy_guard {

	PrivacyState::PrivacyState(const std::chrono::milliseconds liveness_timeout) : liveness_timeout_(liveness_timeout) {}

	void PrivacyState::set_enabled(const bool enabled) {
		// Re-enabling requires every client to publish a fresh explicit state.
		const std::scoped_lock lock(mutex_);
		if (enabled_ == enabled) {
			return;
		}

		enabled_ = enabled;

		if (enabled_) {
			disconnected_clients_ = 0;
			untrusted_failure_latched_ = false;
			for (auto &[connection_id, client] : clients_) {
				static_cast<void>(connection_id);
				client.sensitive.reset();
			}
		}
	}

	bool PrivacyState::enabled() const {
		const std::scoped_lock lock(mutex_);
		return enabled_;
	}

	PrivacyState::ConnectionId PrivacyState::connection_opened(const Clock::time_point now) {
		// New clients remain awaiting until HELLO and STATE both arrive.
		const std::scoped_lock lock(mutex_);
		const ConnectionId connection_id = next_connection_id_++;
		clients_.emplace(connection_id, Client{.last_update = now});
		return connection_id;
	}

	bool PrivacyState::hello_received(const ConnectionId connection_id, std::string client_id, const Clock::time_point now) {
		// Reject repeated handshakes and duplicate live client identities.
		const std::scoped_lock lock(mutex_);
		const auto current = clients_.find(connection_id);
		if (current == clients_.end() || current->second.hello_received) {
			return false;
		}

		for (const auto &[other_connection_id, client] : clients_) {
			if (other_connection_id != connection_id && client.hello_received && client.client_id == client_id) {
				return false;
			}
		}

		current->second.client_id = std::move(client_id);
		current->second.hello_received = true;
		current->second.last_update = now;
		return true;
	}

	bool PrivacyState::state_received(const ConnectionId connection_id, const std::uint64_t sequence, const bool sensitive, const Clock::time_point now) {
		const std::scoped_lock lock(mutex_);
		const auto current = clients_.find(connection_id);
		if (current == clients_.end() || !current->second.hello_received || !accept_sequence(current->second, sequence)) {
			return false;
		}

		const bool initial_state = !current->second.sensitive.has_value();
		current->second.sensitive = sensitive;
		current->second.last_update = now;

		// A fresh, explicit state after a transport failure is the recovery signal.
		// Periodic updates from an existing client cannot recover a different client.
		untrusted_failure_latched_ = false;
		if (initial_state && disconnected_clients_ > 0) {
			--disconnected_clients_;
		}
		return true;
	}

	bool PrivacyState::goodbye_received(const ConnectionId connection_id) {
		const std::scoped_lock lock(mutex_);
		const auto current = clients_.find(connection_id);
		if (current == clients_.end() || !current->second.hello_received) {
			return false;
		}

		clients_.erase(current);
		return true;
	}

	void PrivacyState::protocol_error(const ConnectionId connection_id) {
		const std::scoped_lock lock(mutex_);
		const auto current = clients_.find(connection_id);
		if (current != clients_.end()) {
			record_connection_failure(current->second);
			clients_.erase(current);
		}
	}

	void PrivacyState::connection_closed(const ConnectionId connection_id) {
		const std::scoped_lock lock(mutex_);
		const auto current = clients_.find(connection_id);
		if (current != clients_.end()) {
			record_connection_failure(current->second);
			clients_.erase(current);
		}
	}

	void PrivacyState::tick(const Clock::time_point now) {
		// Clients that stop publishing state are removed and counted as disconnected.
		const std::scoped_lock lock(mutex_);
		for (auto client = clients_.begin(); client != clients_.end();) {
			if (now - client->second.last_update > liveness_timeout_) {
				record_connection_failure(client->second);
				client = clients_.erase(client);
			} else {
				++client;
			}
		}
	}

	bool PrivacyState::accept_sequence(Client &client, const std::uint64_t sequence) {
		// Strictly increasing sequences reject replayed or reordered messages.
		if (client.last_sequence.has_value() && sequence <= *client.last_sequence) {
			return false;
		}

		client.last_sequence = sequence;
		return true;
	}

	void PrivacyState::record_connection_failure(const Client &client) {
		if (client.sensitive.has_value()) {
			++disconnected_clients_;
		} else {
			untrusted_failure_latched_ = true;
		}
	}

	Mode PrivacyState::mode_locked() const {
		// Any uncertain client state wins over safe output.
		if (!enabled_) {
			return Mode::Disabled;
		}
		if (untrusted_failure_latched_ || disconnected_clients_ > 0) {
			return Mode::Disconnected;
		}
		if (clients_.empty()) {
			return Mode::AwaitingConnection;
		}

		bool any_sensitive = false;
		for (const auto &[connection_id, client] : clients_) {
			static_cast<void>(connection_id);
			if (!client.hello_received || !client.sensitive.has_value()) {
				return Mode::AwaitingConnection;
			}
			any_sensitive = any_sensitive || *client.sensitive;
		}

		return any_sensitive ? Mode::Sensitive : Mode::Safe;
	}

	Mode PrivacyState::mode() const {
		const std::scoped_lock lock(mutex_);
		return mode_locked();
	}

} // namespace privacy_guard
