#include "named-pipe-server.hpp"
#include "privacy-state.hpp"
#include "protection-display.hpp"

#include <graphics/vec4.h>
#include <obs-frontend-api.h>
#include <obs-module.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("Privacy Guard contributors")

namespace {

	// Module-owned services live for exactly one OBS plugin load cycle.
	constexpr auto liveness_timeout = std::chrono::seconds(2);
	std::unique_ptr<privacy_guard::PrivacyState> privacy_state;
	std::unique_ptr<privacy_guard::NamedPipeServer> pipe_server;
	std::unique_ptr<privacy_guard::ProtectionDisplayController> display_controller;
	std::optional<privacy_guard::Mode> last_logged_mode;

	const char *mode_name(const privacy_guard::Mode mode) {
		switch (mode) {
		case privacy_guard::Mode::Disabled:
			return "DISABLED";
		case privacy_guard::Mode::AwaitingConnection:
			return "AWAITING_CONNECTION";
		case privacy_guard::Mode::Safe:
			return "SAFE";
		case privacy_guard::Mode::Sensitive:
			return "SENSITIVE";
		case privacy_guard::Mode::Disconnected:
			return "DISCONNECTED";
		}

		return "UNKNOWN";
	}

	// Draw an opaque frame-sized layer after OBS finishes rendering the scene.
	void draw_opaque_overlay(const std::uint32_t width, const std::uint32_t height) {
		gs_effect_t *const effect = obs_get_base_effect(OBS_EFFECT_SOLID);
		if (effect == nullptr) {
			return;
		}

		gs_eparam_t *const color_parameter = gs_effect_get_param_by_name(effect, "color");
		if (color_parameter == nullptr) {
			return;
		}

		vec4 color;
		vec4_set(&color, 0.0F, 0.0F, 0.0F, 1.0F);
		gs_effect_set_vec4(color_parameter, &color);

		while (gs_effect_loop(effect, "Solid")) {
			gs_draw_sprite(nullptr, 0, width, height);
		}
	}

	void render_privacy_overlay(void *) {
		if (display_controller == nullptr) {
			return;
		}

		// Custom outputs retain their scene or image and receive a post-render watermark.
		const bool overlay_required = display_controller->overlay_required();
		const bool watermark_required = display_controller->watermark_required();
		if (!overlay_required && !watermark_required) {
			return;
		}

		obs_video_info video_info{};
		if (obs_get_video_info(&video_info)) {
			if (overlay_required) {
				draw_opaque_overlay(video_info.base_width, video_info.base_height);
				display_controller->render_privacy_image(video_info.base_width, video_info.base_height);
			}
			if (watermark_required) {
				display_controller->render_watermark(video_info.base_width, video_info.base_height);
			}
		}
	}

	void tick_privacy_state(void *, float) {
		// Advance client liveness timeouts and forward mode transitions once per OBS tick.
		if (privacy_state != nullptr) {
			privacy_state->tick(privacy_guard::PrivacyState::Clock::now());
			const privacy_guard::Mode current_mode = privacy_state->mode();
			if (!last_logged_mode.has_value() || *last_logged_mode != current_mode) {
				blog(LOG_INFO, "[OBS VS Code Privacy Guard] State changed to %s", mode_name(current_mode));
				last_logged_mode = current_mode;
				if (display_controller != nullptr) {
					display_controller->privacy_mode_changed(current_mode);
				}
			}
		}
	}

	// Thin C callbacks bridge OBS frontend APIs to the C++ controller.
	void show_settings(void *const private_data) {
		static_cast<privacy_guard::ProtectionDisplayController *>(private_data)->show_settings_dialog();
	}

	void save_load_settings(obs_data_t *const save_data, const bool saving, void *const private_data) {
		static_cast<privacy_guard::ProtectionDisplayController *>(private_data)->save_load(save_data, saving);
	}

	void frontend_event(const obs_frontend_event event, void *const private_data) {
		static_cast<privacy_guard::ProtectionDisplayController *>(private_data)->frontend_event(event);
	}

} // namespace

const char *obs_module_description() {
	return "Fails closed when a sensitive file is visible in VS Code.";
}

bool obs_module_load() {
	// Register rendering, frontend, persistence, and IPC services with OBS.
	privacy_state = std::make_unique<privacy_guard::PrivacyState>(liveness_timeout);
	display_controller = std::make_unique<privacy_guard::ProtectionDisplayController>(*privacy_state);
	pipe_server =
		std::make_unique<privacy_guard::NamedPipeServer>(*privacy_state, [](const privacy_guard::PipeLogLevel level, const std::string_view operation, const std::uint32_t error) {
			blog(level == privacy_guard::PipeLogLevel::Error ? LOG_ERROR : LOG_WARNING, "[OBS VS Code Privacy Guard] %.*s failed with error %lu",
				static_cast<int>(operation.size()), operation.data(), static_cast<unsigned long>(error));
		});

	obs_add_tick_callback(tick_privacy_state, nullptr);
	obs_add_main_rendered_callback(render_privacy_overlay, nullptr);
	obs_frontend_add_tools_menu_item("VS Code Privacy Guard", show_settings, display_controller.get());
	obs_frontend_add_save_callback(save_load_settings, display_controller.get());
	obs_frontend_add_event_callback(frontend_event, display_controller.get());
	pipe_server->start();

	blog(LOG_INFO, "[OBS VS Code Privacy Guard] Loaded and listening for VS Code in fail-closed awaiting state");
	return true;
}

void obs_module_unload() {
	// Stop IPC before removing callbacks and destroying shared state.
	pipe_server.reset();
	obs_frontend_remove_event_callback(frontend_event, display_controller.get());
	obs_frontend_remove_save_callback(save_load_settings, display_controller.get());
	obs_remove_main_rendered_callback(render_privacy_overlay, nullptr);
	obs_remove_tick_callback(tick_privacy_state, nullptr);
	display_controller.reset();
	privacy_state.reset();
	last_logged_mode.reset();

	blog(LOG_INFO, "[OBS VS Code Privacy Guard] Unloaded");
}
