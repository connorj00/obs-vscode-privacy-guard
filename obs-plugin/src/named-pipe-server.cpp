#include "named-pipe-server.hpp"

#include "protocol.hpp"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace privacy_guard {
	namespace {

		// Windows transport limits and polling cadence.
		constexpr wchar_t pipe_path[] = LR"(\\.\pipe\obs-vscode-privacy-guard-v1)";
		constexpr DWORD pipe_buffer_size = 4'096;
		constexpr auto poll_interval = std::chrono::milliseconds(20);

		enum class FrameResult {
			Continue,
			Goodbye,
			Invalid,
		};

		// Apply a validated protocol frame to the fail-closed state machine.
		FrameResult apply_frame(PrivacyState &privacy_state, const PrivacyState::ConnectionId connection_id, const Frame &frame) {
			const PrivacyState::Clock::time_point now = PrivacyState::Clock::now();

			switch (frame.kind) {
			case FrameKind::Hello:
				return privacy_state.hello_received(connection_id, frame.client_id, now) ? FrameResult::Continue : FrameResult::Invalid;
			case FrameKind::State:
				return privacy_state.state_received(connection_id, frame.sequence, frame.sensitive, now) ? FrameResult::Continue : FrameResult::Invalid;
			case FrameKind::Goodbye:
				return privacy_state.goodbye_received(connection_id) ? FrameResult::Goodbye : FrameResult::Invalid;
			}

			return FrameResult::Invalid;
		}

	} // namespace

	NamedPipeServer::NamedPipeServer(PrivacyState &privacy_state, PipeLogCallback log_callback) : privacy_state_(privacy_state), log_callback_(std::move(log_callback)) {}

	NamedPipeServer::~NamedPipeServer() {
		stop();
	}

	void NamedPipeServer::start() {
		// Starting is idempotent so OBS lifecycle callbacks cannot duplicate listeners.
		bool expected = false;
		if (!running_.compare_exchange_strong(expected, true)) {
			return;
		}

		accept_thread_ = std::jthread([this] { accept_connections(); });
	}

	void NamedPipeServer::stop() {
		// Wake the blocking accept call before joining all background workers.
		if (!running_.exchange(false)) {
			return;
		}

		wake_accept_thread();
		if (accept_thread_.joinable()) {
			accept_thread_.join();
		}

		// Workers poll their pipe and observe running_ within a few milliseconds.
		workers_.clear();
	}

	void NamedPipeServer::accept_connections() {
		// Create a fresh local-only pipe instance for each VS Code connection.
		while (running_) {
			HANDLE pipe = CreateNamedPipeW(pipe_path, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, PIPE_UNLIMITED_INSTANCES,
				pipe_buffer_size, pipe_buffer_size, 0, nullptr);

			if (pipe == INVALID_HANDLE_VALUE) {
				if (log_callback_) {
					log_callback_(PipeLogLevel::Error, "CreateNamedPipe", GetLastError());
				}
				std::this_thread::sleep_for(std::chrono::seconds(1));
				continue;
			}

			const BOOL connected = ConnectNamedPipe(pipe, nullptr);
			const DWORD connection_error = connected ? ERROR_SUCCESS : GetLastError();
			if (!connected && connection_error != ERROR_PIPE_CONNECTED) {
				CloseHandle(pipe);
				if (running_ && log_callback_) {
					log_callback_(PipeLogLevel::Warning, "ConnectNamedPipe", connection_error);
				}
				continue;
			}

			if (!running_) {
				DisconnectNamedPipe(pipe);
				CloseHandle(pipe);
				break;
			}

			const PrivacyState::ConnectionId connection_id = privacy_state_.connection_opened(PrivacyState::Clock::now());
			remove_completed_workers();
			auto finished = std::make_shared<std::atomic_bool>(false);
			workers_.push_back(Worker{finished, std::jthread([this, pipe, connection_id, finished] {
				process_connection(pipe, connection_id);
				finished->store(true, std::memory_order_relaxed);
			})});
		}
	}

	void NamedPipeServer::process_connection(void *const raw_pipe, const PrivacyState::ConnectionId connection_id) {
		// Buffer byte-stream reads until complete newline-delimited frames arrive.
		HANDLE const pipe = static_cast<HANDLE>(raw_pipe);
		std::array<char, pipe_buffer_size> read_buffer{};
		std::string pending;
		bool graceful = false;
		bool state_entry_active = true;
		const auto fail_protocol = [&] {
			privacy_state_.protocol_error(connection_id);
			state_entry_active = false;
			graceful = true;
		};

		while (running_ && !graceful) {
			DWORD available = 0;
			if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr)) {
				break;
			}
			if (available == 0) {
				std::this_thread::sleep_for(poll_interval);
				continue;
			}

			DWORD bytes_read = 0;
			const DWORD requested = std::min<DWORD>(available, static_cast<DWORD>(read_buffer.size()));
			if (!ReadFile(pipe, read_buffer.data(), requested, &bytes_read, nullptr) || bytes_read == 0) {
				break;
			}

			pending.append(read_buffer.data(), bytes_read);

			std::size_t newline = 0;
			// Validate and apply every complete frame currently buffered.
			while ((newline = pending.find('\n')) != std::string::npos) {
				const std::string_view line(pending.data(), newline);
				const std::optional<Frame> frame = parse_frame(line);
				if (!frame.has_value()) {
					fail_protocol();
					break;
				}

				const FrameResult result = apply_frame(privacy_state_, connection_id, *frame);
				if (result == FrameResult::Invalid) {
					fail_protocol();
					break;
				}

				pending.erase(0, newline + 1);
				if (result == FrameResult::Goodbye) {
					state_entry_active = false;
					graceful = true;
					break;
				}
			}

			// Reject an oversized partial frame even when it followed valid frames.
			if (!graceful && pending.size() > max_frame_size) {
				fail_protocol();
			}
		}

		if (state_entry_active && running_) {
			privacy_state_.connection_closed(connection_id);
		}

		DisconnectNamedPipe(pipe);
		CloseHandle(pipe);
	}

	void NamedPipeServer::remove_completed_workers() {
		std::erase_if(workers_, [](const Worker &worker) { return worker.finished->load(std::memory_order_relaxed); });
	}

	void NamedPipeServer::wake_accept_thread() const {
		// A short-lived client releases ConnectNamedPipe during plugin shutdown.
		for (int attempt = 0; attempt < 10; ++attempt) {
			HANDLE const client = CreateFileW(pipe_path, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (client != INVALID_HANDLE_VALUE) {
				CloseHandle(client);
				return;
			}
			std::this_thread::sleep_for(poll_interval);
		}
	}

} // namespace privacy_guard
