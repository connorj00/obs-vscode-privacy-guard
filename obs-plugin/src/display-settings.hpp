#pragma once

#include "privacy-state.hpp"

#include <string>

namespace privacy_guard {

	enum class DisplayMode {
		BuiltInImage,
		CustomImage,
		CustomScene,
	};

	struct EventDisplaySettings final {
		DisplayMode mode{DisplayMode::BuiltInImage};
		std::string image_path;
		std::string scene;
	};

	// Per-scene-collection choices made in the OBS Tools dialog.
	struct DisplaySettings final {
		EventDisplaySettings sensitive;
		EventDisplaySettings connection;
		bool custom_scene_risk_accepted{false};
	};

	enum class DisplayAction {
		NormalOutput,
		BuiltInImage,
		CustomImage,
		CustomScene,
	};

	// Resolve a privacy mode while preserving the bundled image as the safe default.
	[[nodiscard]] constexpr DisplayAction resolve_display_action(const DisplaySettings &settings, const Mode mode) {
		if (mode == Mode::Safe || mode == Mode::Disabled) {
			return DisplayAction::NormalOutput;
		}

		const EventDisplaySettings &event = mode == Mode::Sensitive ? settings.sensitive : settings.connection;
		if (event.mode == DisplayMode::CustomScene && settings.custom_scene_risk_accepted && !event.scene.empty()) {
			return DisplayAction::CustomScene;
		}
		if (event.mode == DisplayMode::CustomImage && !event.image_path.empty()) {
			return DisplayAction::CustomImage;
		}

		return DisplayAction::BuiltInImage;
	}

} // namespace privacy_guard
