#include "protocol.hpp"

#include <array>
#include <charconv>
#include <cstddef>
#include <vector>

namespace privacy_guard {
	namespace {

		// Split strictly on single spaces; empty tokens make a frame invalid.
		std::vector<std::string_view> split_tokens(const std::string_view line) {
			std::vector<std::string_view> tokens;
			std::size_t position = 0;

			while (position < line.size()) {
				const std::size_t separator = line.find(' ', position);
				const std::size_t length = separator == std::string_view::npos ? line.size() - position : separator - position;
				if (length == 0) {
					return {};
				}

				tokens.emplace_back(line.substr(position, length));
				if (separator == std::string_view::npos) {
					break;
				}
				position = separator + 1;
			}

			return tokens;
		}

		bool is_hexadecimal(const char character) {
			return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f') || (character >= 'A' && character <= 'F');
		}

		bool is_uuid(const std::string_view value) {
			// Client IDs use the canonical 8-4-4-4-12 UUID shape.
			constexpr std::array<std::size_t, 4> separators{8, 13, 18, 23};
			if (value.size() != 36) {
				return false;
			}

			for (std::size_t index = 0; index < value.size(); ++index) {
				const bool separator = index == separators[0] || index == separators[1] || index == separators[2] || index == separators[3];
				if (separator ? value[index] != '-' : !is_hexadecimal(value[index])) {
					return false;
				}
			}

			return true;
		}

		std::optional<std::uint64_t> parse_sequence(const std::string_view value) {
			// from_chars rejects signs, whitespace, and partially parsed sequences.
			std::uint64_t sequence = 0;
			const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), sequence);
			if (error != std::errc{} || end != value.data() + value.size()) {
				return std::nullopt;
			}
			return sequence;
		}

	} // namespace

	std::optional<Frame> parse_frame(const std::string_view line) {
		// Only exact PG/1 frame shapes are accepted; unknown input fails closed.
		if (line.empty() || line.size() > max_frame_size || line.front() == ' ' || line.back() == ' ' || line.back() == '\r') {
			return std::nullopt;
		}

		const std::vector<std::string_view> tokens = split_tokens(line);
		if (tokens.size() < 2 || tokens[0] != "PG/1") {
			return std::nullopt;
		}

		if (tokens[1] == "HELLO") {
			if (tokens.size() != 3 || !is_uuid(tokens[2])) {
				return std::nullopt;
			}
			return Frame{FrameKind::Hello, std::string(tokens[2])};
		}

		if (tokens[1] == "STATE") {
			if (tokens.size() != 4) {
				return std::nullopt;
			}
			const std::optional<std::uint64_t> sequence = parse_sequence(tokens[2]);
			if (!sequence.has_value() || (tokens[3] != "SAFE" && tokens[3] != "SENSITIVE")) {
				return std::nullopt;
			}
			return Frame{FrameKind::State, {}, *sequence, tokens[3] == "SENSITIVE"};
		}

		if (tokens[1] == "GOODBYE" && tokens.size() == 2) {
			return Frame{FrameKind::Goodbye};
		}

		return std::nullopt;
	}

} // namespace privacy_guard
