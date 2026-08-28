#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace privacy_guard {

	// Hard limit prevents an untrusted local client from growing buffers forever.
	constexpr std::size_t max_frame_size = 1'024;

	enum class FrameKind {
		Hello,
		State,
		Goodbye,
	};

	// Parsed representation shared by the transport and privacy state machine.
	struct Frame final {
		FrameKind kind;
		std::string client_id;
		std::uint64_t sequence{};
		bool sensitive{};
	};

	[[nodiscard]] std::optional<Frame> parse_frame(std::string_view line);

} // namespace privacy_guard
