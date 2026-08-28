#pragma once

#include "privacy-state.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
#include <thread>
#include <vector>

namespace privacy_guard {

	enum class PipeLogLevel {
		Warning,
		Error,
	};

	using PipeLogCallback = std::function<void(PipeLogLevel, std::string_view, std::uint32_t)>;

	// Accepts local VS Code clients and dispatches each connection to a worker.
	class NamedPipeServer final {
	  public:
		explicit NamedPipeServer(PrivacyState &privacy_state, PipeLogCallback log_callback = {});
		~NamedPipeServer();

		NamedPipeServer(const NamedPipeServer &) = delete;
		NamedPipeServer &operator=(const NamedPipeServer &) = delete;

		void start();
		void stop();

	  private:
		struct Worker final {
			std::shared_ptr<std::atomic_bool> finished;
			std::jthread thread;
		};

		// The accept loop owns pipe creation; workers own connected pipe handles.
		void accept_connections();
		void process_connection(void *pipe, PrivacyState::ConnectionId connection_id);
		void remove_completed_workers();
		void wake_accept_thread() const;

		PrivacyState &privacy_state_;
		PipeLogCallback log_callback_;
		std::atomic_bool running_{false};
		std::jthread accept_thread_;
		std::vector<Worker> workers_;
	};

} // namespace privacy_guard
