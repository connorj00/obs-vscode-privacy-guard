#include "display-settings.hpp"

namespace privacy_guard {
	namespace {

		// Compile-time checks prevent future settings changes from weakening defaults.
		constexpr bool secure_defaults_are_preserved() {
			DisplaySettings settings;
			if (resolve_display_action(settings, Mode::Sensitive) != DisplayAction::BuiltInImage ||
				resolve_display_action(settings, Mode::Disconnected) != DisplayAction::BuiltInImage) {
				return false;
			}

			settings.sensitive.mode = DisplayMode::CustomImage;
			if (resolve_display_action(settings, Mode::Sensitive) != DisplayAction::BuiltInImage) {
				return false;
			}

			settings.sensitive.image_path = "privacy.png";
			if (resolve_display_action(settings, Mode::Sensitive) != DisplayAction::CustomImage) {
				return false;
			}

			settings.sensitive.mode = DisplayMode::CustomScene;
			settings.sensitive.scene = "Privacy Scene";
			if (resolve_display_action(settings, Mode::Sensitive) != DisplayAction::BuiltInImage) {
				return false;
			}

			settings.custom_scene_risk_accepted = true;
			return resolve_display_action(settings, Mode::Sensitive) == DisplayAction::CustomScene && resolve_display_action(settings, Mode::Safe) == DisplayAction::NormalOutput;
		}

		static_assert(secure_defaults_are_preserved(), "Custom display policy must remain secure by default");

	} // namespace
} // namespace privacy_guard
