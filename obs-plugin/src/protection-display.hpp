#pragma once

#include "display-settings.hpp"

#include <obs-frontend-api.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

struct obs_data;

namespace privacy_guard {
	class PrivacyState;

	// Coordinates privacy modes, custom scenes, persistence, and black fallback.
	class ProtectionDisplayController final {
	  public:
		explicit ProtectionDisplayController(PrivacyState &privacy_state);
		~ProtectionDisplayController();

		ProtectionDisplayController(const ProtectionDisplayController &) = delete;
		ProtectionDisplayController &operator=(const ProtectionDisplayController &) = delete;

		void privacy_mode_changed(Mode mode);
		[[nodiscard]] bool overlay_required() const;
		[[nodiscard]] bool watermark_required() const;
		void render_privacy_image(std::uint32_t width, std::uint32_t height);
		void render_watermark(std::uint32_t width, std::uint32_t height);

		void show_settings_dialog();
		void save_load(obs_data *save_data, bool saving);
		void frontend_event(obs_frontend_event event);

	  private:
		// UI work is generation-tagged so stale queued updates can be ignored.
		struct ApplyRequest;
		struct ImageRenderer;

		static void apply_task(void *request_data);
		static void barrier_task(void *);

		void queue_apply(Mode mode);
		void apply_on_ui_thread(Mode mode, std::uint64_t generation);
		void restore_previous_scene();
		void shutdown();
		void update_settings(DisplaySettings settings);
		[[nodiscard]] DisplaySettings settings_snapshot() const;

		PrivacyState &privacy_state_;
		mutable std::mutex mutex_;
		DisplaySettings settings_;
		Mode latest_mode_{Mode::AwaitingConnection};
		std::uint64_t generation_{0};
		std::atomic_bool overlay_required_{true};
		std::atomic_bool watermark_required_{false};
		std::atomic_bool alive_{true};
		std::unique_ptr<ImageRenderer> image_renderer_;
		std::unique_ptr<ImageRenderer> watermark_renderer_;
		bool watermark_available_{false};

		// Scene bookkeeping is accessed only through OBS's UI task queue.
		bool custom_scene_active_{false};
		std::string previous_scene_;
		std::string active_target_scene_;
	};

} // namespace privacy_guard
