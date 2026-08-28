#include "protection-display.hpp"

#include "privacy-state.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/bmem.h>

#include <QAbstractButton>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QObject>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>
#include <vector>

namespace privacy_guard {
	namespace {

		// Keys stored inside the active OBS scene collection.
		constexpr char settings_key[] = "obs-vscode-privacy-guard";
		constexpr char sensitive_mode_key[] = "sensitive_mode";
		constexpr char sensitive_image_key[] = "sensitive_image";
		constexpr char sensitive_scene_key[] = "sensitive_scene";
		constexpr char connection_mode_key[] = "connection_mode";
		constexpr char connection_image_key[] = "connection_image";
		constexpr char connection_scene_key[] = "connection_scene";
		constexpr char risk_accepted_key[] = "custom_scene_risk_accepted";
		constexpr char legacy_use_sensitive_scene_key[] = "use_sensitive_scene";
		constexpr char legacy_use_connection_scene_key[] = "use_connection_scene";

		constexpr char sensitive_resource_path[] = ":/privacy-guard/sensitive-file.png";
		constexpr char connection_resource_path[] = ":/privacy-guard/connection-lost.png";
		constexpr char watermark_resource_path[] = ":/privacy-guard/watermark.png";

		bool is_protected_mode(const Mode mode) {
			return mode == Mode::AwaitingConnection || mode == Mode::Sensitive || mode == Mode::Disconnected;
		}

		DisplayMode selected_display_mode(const QComboBox &combo) {
			return static_cast<DisplayMode>(combo.currentData().toInt());
		}

		void populate_display_modes(QComboBox &combo, const DisplayMode selected) {
			combo.addItem("Built-in Privacy Guard image (recommended)", static_cast<int>(DisplayMode::BuiltInImage));
			combo.addItem("Custom image (post-render protected)", static_cast<int>(DisplayMode::CustomImage));
			combo.addItem("Custom OBS scene (user managed)", static_cast<int>(DisplayMode::CustomScene));

			const int index = combo.findData(static_cast<int>(selected));
			combo.setCurrentIndex(index >= 0 ? index : 0);
		}

		QImage load_privacy_image(const QString &path) {
			QImageReader reader(path);
			reader.setAutoTransform(true);
			const QImage image = reader.read();
			return image.isNull() ? QImage{} : image.convertToFormat(QImage::Format_RGBA8888);
		}

		DisplayMode load_display_mode(obs_data_t *const data, const char *const key, const char *const legacy_scene_key) {
			if (!obs_data_has_user_value(data, key)) {
				return obs_data_get_bool(data, legacy_scene_key) ? DisplayMode::CustomScene : DisplayMode::BuiltInImage;
			}

			const long long stored = obs_data_get_int(data, key);
			if (stored < static_cast<long long>(DisplayMode::BuiltInImage) || stored > static_cast<long long>(DisplayMode::CustomScene)) {
				return DisplayMode::BuiltInImage;
			}
			return static_cast<DisplayMode>(stored);
		}

		// OBS scene helpers keep reference acquisition and release balanced.
		std::vector<std::string> scene_names() {
			std::vector<std::string> names;
			char **const scenes = obs_frontend_get_scene_names();
			if (scenes == nullptr) {
				return names;
			}

			for (char **scene = scenes; *scene != nullptr; ++scene) {
				names.emplace_back(*scene);
			}
			bfree(scenes);
			return names;
		}

		bool scene_exists(const std::string &name) {
			obs_source_t *const source = obs_get_source_by_name(name.c_str());
			if (source == nullptr) {
				return false;
			}

			const bool is_scene = obs_scene_from_source(source) != nullptr;
			obs_source_release(source);
			return is_scene;
		}

		bool switch_to_scene(const std::string &name) {
			obs_source_t *const scene = obs_get_source_by_name(name.c_str());
			if (scene == nullptr || obs_scene_from_source(scene) == nullptr) {
				if (scene != nullptr) {
					obs_source_release(scene);
				}
				return false;
			}

			obs_frontend_set_current_scene(scene);
			obs_source_release(scene);
			return true;
		}

		std::string current_scene_name() {
			obs_source_t *const scene = obs_frontend_get_current_scene();
			if (scene == nullptr) {
				return {};
			}

			const char *const name = obs_source_get_name(scene);
			const std::string result = name != nullptr ? name : "";
			obs_source_release(scene);
			return result;
		}

		void populate_scene_combo(QComboBox &combo, const std::string &selected) {
			bool selected_found = false;
			for (const std::string &name : scene_names()) {
				combo.addItem(QString::fromStdString(name), QString::fromStdString(name));
				selected_found = selected_found || name == selected;
			}

			if (!selected.empty() && !selected_found) {
				combo.addItem(QString("Missing: %1").arg(QString::fromStdString(selected)), QString::fromStdString(selected));
			}

			const int selected_index = combo.findData(QString::fromStdString(selected));
			if (selected_index >= 0) {
				combo.setCurrentIndex(selected_index);
			}
		}

	} // namespace

	struct ProtectionDisplayController::ApplyRequest final {
		// Carries state safely from OBS's tick thread to its UI task queue.
		ProtectionDisplayController *controller;
		Mode mode;
		std::uint64_t generation;
	};

	struct ProtectionDisplayController::ImageRenderer final {
		// Image selection may occur on the UI thread while rendering uses OBS graphics.
		void clear() {
			select({});
		}

		bool select_default(const Mode mode) {
			const char *const resource = mode == Mode::Sensitive ? sensitive_resource_path : connection_resource_path;
			QImage image = load_privacy_image(QString::fromUtf8(resource));
			if (image.isNull()) {
				blog(LOG_ERROR, "[OBS VS Code Privacy Guard] Bundled privacy image '%s' could not be loaded; using black fallback", resource);
				clear();
				return false;
			}

			select(std::move(image));
			return true;
		}

		bool select_custom(const std::string &path) {
			QImage image = load_privacy_image(QString::fromStdString(path));
			if (image.isNull()) {
				return false;
			}

			select(std::move(image));
			return true;
		}

		bool select_resource(const char *const path) {
			QImage image = load_privacy_image(QString::fromUtf8(path));
			if (image.isNull()) {
				return false;
			}

			select(std::move(image));
			return true;
		}

		void render(const std::uint32_t canvas_width, const std::uint32_t canvas_height) {
			render_with_layout(canvas_width, canvas_height, false);
		}

		void render_full_canvas(const std::uint32_t canvas_width, const std::uint32_t canvas_height) {
			render_with_layout(canvas_width, canvas_height, true);
		}

		void shutdown() {
			obs_enter_graphics();
			if (texture_ != nullptr) {
				gs_texture_destroy(texture_);
				texture_ = nullptr;
			}
			obs_leave_graphics();
		}

	  private:
		void render_with_layout(const std::uint32_t canvas_width, const std::uint32_t canvas_height, const bool stretch_to_canvas) {
			update_texture();
			if (texture_ == nullptr || texture_width_ == 0 || texture_height_ == 0 || canvas_width == 0 || canvas_height == 0) {
				return;
			}

			gs_effect_t *const effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
			if (effect == nullptr) {
				return;
			}

			gs_eparam_t *const image_parameter = gs_effect_get_param_by_name(effect, "image");
			if (image_parameter == nullptr) {
				return;
			}

			std::uint32_t draw_width = canvas_width;
			std::uint32_t draw_height = canvas_height;
			float offset_x = 0.0F;
			float offset_y = 0.0F;
			if (!stretch_to_canvas) {
				const float scale =
					std::min(static_cast<float>(canvas_width) / static_cast<float>(texture_width_), static_cast<float>(canvas_height) / static_cast<float>(texture_height_));
				draw_width = std::max(1U, static_cast<std::uint32_t>(std::lround(static_cast<float>(texture_width_) * scale)));
				draw_height = std::max(1U, static_cast<std::uint32_t>(std::lround(static_cast<float>(texture_height_) * scale)));
				offset_x = static_cast<float>(canvas_width - draw_width) / 2.0F;
				offset_y = static_cast<float>(canvas_height - draw_height) / 2.0F;
			}

			gs_effect_set_texture(image_parameter, texture_);
			gs_blend_state_push();
			gs_blend_function(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA);
			gs_matrix_push();
			gs_matrix_translate3f(offset_x, offset_y, 0.0F);
			while (gs_effect_loop(effect, "Draw")) {
				gs_draw_sprite(texture_, 0, draw_width, draw_height);
			}
			gs_matrix_pop();
			gs_blend_state_pop();
		}

		void select(QImage image) {
			const std::scoped_lock lock(mutex_);
			selected_image_ = std::move(image);
			++selected_generation_;
		}

		void update_texture() {
			QImage selected;
			std::uint64_t generation = 0;
			{
				const std::scoped_lock lock(mutex_);
				if (texture_generation_ == selected_generation_) {
					return;
				}
				selected = selected_image_;
				generation = selected_generation_;
			}

			if (texture_ != nullptr) {
				gs_texture_destroy(texture_);
				texture_ = nullptr;
			}
			texture_width_ = 0;
			texture_height_ = 0;

			if (!selected.isNull()) {
				const std::uint8_t *pixels = selected.constBits();
				texture_ = gs_texture_create(static_cast<std::uint32_t>(selected.width()), static_cast<std::uint32_t>(selected.height()), GS_RGBA, 1, &pixels, 0);
				if (texture_ != nullptr) {
					texture_width_ = static_cast<std::uint32_t>(selected.width());
					texture_height_ = static_cast<std::uint32_t>(selected.height());
				}
			}

			const std::scoped_lock lock(mutex_);
			texture_generation_ = generation;
		}

		std::mutex mutex_;
		QImage selected_image_;
		std::uint64_t selected_generation_{1};
		std::uint64_t texture_generation_{0};
		gs_texture_t *texture_{nullptr};
		std::uint32_t texture_width_{0};
		std::uint32_t texture_height_{0};
	};

	ProtectionDisplayController::ProtectionDisplayController(PrivacyState &privacy_state)
		: privacy_state_(privacy_state), image_renderer_(std::make_unique<ImageRenderer>()), watermark_renderer_(std::make_unique<ImageRenderer>()) {
		watermark_available_ = watermark_renderer_->select_resource(watermark_resource_path);
		if (!watermark_available_) {
			blog(LOG_ERROR, "[OBS VS Code Privacy Guard] Bundled watermark could not be loaded; custom outputs will continue without it");
		}
	}

	ProtectionDisplayController::~ProtectionDisplayController() {
		shutdown();
	}

	void ProtectionDisplayController::privacy_mode_changed(const Mode mode) {
		// Protected modes cover output synchronously before any queued scene work.
		if (!alive_) {
			return;
		}

		if (is_protected_mode(mode)) {
			// Cover with black before the UI thread selects an image or custom scene.
			watermark_required_ = false;
			overlay_required_ = true;
			image_renderer_->clear();
		}
		queue_apply(mode);
	}

	bool ProtectionDisplayController::overlay_required() const {
		return overlay_required_;
	}

	bool ProtectionDisplayController::watermark_required() const {
		return watermark_required_;
	}

	void ProtectionDisplayController::render_privacy_image(const std::uint32_t width, const std::uint32_t height) {
		image_renderer_->render(width, height);
	}

	void ProtectionDisplayController::render_watermark(const std::uint32_t width, const std::uint32_t height) {
		watermark_renderer_->render_full_canvas(width, height);
	}

	void ProtectionDisplayController::queue_apply(const Mode mode) {
		// Later mode changes supersede earlier UI tasks through the generation value.
		std::uint64_t generation = 0;
		{
			const std::scoped_lock lock(mutex_);
			latest_mode_ = mode;
			generation = ++generation_;
		}

		auto *const request = new ApplyRequest{this, mode, generation};
		obs_queue_task(OBS_TASK_UI, apply_task, request, false);
	}

	void ProtectionDisplayController::apply_task(void *const request_data) {
		const std::unique_ptr<ApplyRequest> request(static_cast<ApplyRequest *>(request_data));
		request->controller->apply_on_ui_thread(request->mode, request->generation);
	}

	void ProtectionDisplayController::barrier_task(void *) {}

	void ProtectionDisplayController::apply_on_ui_thread(const Mode mode, const std::uint64_t generation) {
		// All OBS frontend scene access is confined to the UI thread.
		if (!alive_) {
			return;
		}

		{
			const std::scoped_lock lock(mutex_);
			if (generation != generation_) {
				return;
			}
		}

		if (!is_protected_mode(mode)) {
			watermark_required_ = false;
			overlay_required_ = false;
			restore_previous_scene();
			return;
		}

		const DisplaySettings settings = settings_snapshot();
		const EventDisplaySettings &event = mode == Mode::Sensitive ? settings.sensitive : settings.connection;
		const DisplayAction action = resolve_display_action(settings, mode);

		if (action == DisplayAction::BuiltInImage) {
			restore_previous_scene();
			image_renderer_->select_default(mode);
			watermark_required_ = false;
			overlay_required_ = true;
			return;
		}

		if (action == DisplayAction::CustomImage) {
			restore_previous_scene();
			if (!image_renderer_->select_custom(event.image_path)) {
				image_renderer_->select_default(mode);
				watermark_required_ = false;
				blog(LOG_WARNING,
					"[OBS VS Code Privacy Guard] Custom image '%s' is unavailable; "
					"using the bundled fallback",
					event.image_path.c_str());
			} else {
				watermark_required_ = watermark_available_;
			}
			overlay_required_ = true;
			return;
		}

		const std::string previous_scene = custom_scene_active_ ? std::string{} : current_scene_name();
		if (!switch_to_scene(event.scene)) {
			restore_previous_scene();
			image_renderer_->select_default(mode);
			watermark_required_ = false;
			overlay_required_ = true;
			blog(LOG_WARNING,
				"[OBS VS Code Privacy Guard] Custom scene '%s' is unavailable; "
				"using the bundled fallback",
				event.scene.c_str());
			return;
		}

		if (!custom_scene_active_) {
			previous_scene_ = previous_scene;
			custom_scene_active_ = true;
		}
		image_renderer_->clear();
		active_target_scene_ = event.scene;
		watermark_required_ = watermark_available_;
		overlay_required_ = false;
		blog(LOG_INFO, "[OBS VS Code Privacy Guard] Switched to user-managed scene '%s'", event.scene.c_str());
	}

	void ProtectionDisplayController::restore_previous_scene() {
		if (custom_scene_active_ && !previous_scene_.empty() && !switch_to_scene(previous_scene_)) {
			blog(LOG_WARNING,
				"[OBS VS Code Privacy Guard] Previous scene '%s' is unavailable; "
				"leaving the current scene active",
				previous_scene_.c_str());
		}

		custom_scene_active_ = false;
		previous_scene_.clear();
		active_target_scene_.clear();
	}

	void ProtectionDisplayController::show_settings_dialog() {
		// Build the per-scene-collection configuration dialog from current settings.
		const DisplaySettings current = settings_snapshot();
		auto *const parent = static_cast<QWidget *>(obs_frontend_get_main_window());
		QDialog dialog(parent);
		dialog.setWindowTitle("VS Code Privacy Guard");
		dialog.setMinimumWidth(680);

		QVBoxLayout layout(&dialog);
		QLabel introduction("Privacy Guard uses a bundled post-render image by default. Custom images "
							"retain the same protection; custom OBS scenes are user managed.",
			&dialog);
		introduction.setWordWrap(true);
		layout.addWidget(&introduction);

		QCheckBox protection_enabled("Enable Privacy Guard protection", &dialog);
		protection_enabled.setChecked(privacy_state_.enabled());
		layout.addWidget(&protection_enabled);

		QGroupBox sensitive_group("Sensitive file", &dialog);
		QVBoxLayout sensitive_group_layout(&sensitive_group);
		QComboBox sensitive_mode(&sensitive_group);
		populate_display_modes(sensitive_mode, current.sensitive.mode);
		sensitive_group_layout.addWidget(&sensitive_mode);

		QWidget sensitive_image_row(&sensitive_group);
		QHBoxLayout sensitive_image_layout(&sensitive_image_row);
		sensitive_image_layout.setContentsMargins(24, 0, 0, 0);
		QLabel sensitive_image_label("Image:", &sensitive_image_row);
		sensitive_image_label.setMinimumWidth(64);
		QLineEdit sensitive_image(QString::fromStdString(current.sensitive.image_path), &sensitive_image_row);
		QPushButton sensitive_browse("Browse...", &sensitive_image_row);
		sensitive_image_layout.addWidget(&sensitive_image_label);
		sensitive_image_layout.addWidget(&sensitive_image);
		sensitive_image_layout.addWidget(&sensitive_browse);
		sensitive_group_layout.addWidget(&sensitive_image_row);

		QWidget sensitive_scene_row(&sensitive_group);
		QHBoxLayout sensitive_scene_layout(&sensitive_scene_row);
		sensitive_scene_layout.setContentsMargins(24, 0, 0, 0);
		QLabel sensitive_scene_label("Scene:", &sensitive_scene_row);
		sensitive_scene_label.setMinimumWidth(64);
		QComboBox sensitive_scene(&sensitive_scene_row);
		populate_scene_combo(sensitive_scene, current.sensitive.scene);
		sensitive_scene_layout.addWidget(&sensitive_scene_label);
		sensitive_scene_layout.addWidget(&sensitive_scene);
		sensitive_group_layout.addWidget(&sensitive_scene_row);

		QGroupBox connection_group("VS Code unavailable", &dialog);
		QVBoxLayout connection_group_layout(&connection_group);
		QComboBox connection_mode(&connection_group);
		populate_display_modes(connection_mode, current.connection.mode);
		connection_group_layout.addWidget(&connection_mode);

		QWidget connection_image_row(&connection_group);
		QHBoxLayout connection_image_layout(&connection_image_row);
		connection_image_layout.setContentsMargins(24, 0, 0, 0);
		QLabel connection_image_label("Image:", &connection_image_row);
		connection_image_label.setMinimumWidth(64);
		QLineEdit connection_image(QString::fromStdString(current.connection.image_path), &connection_image_row);
		QPushButton connection_browse("Browse...", &connection_image_row);
		connection_image_layout.addWidget(&connection_image_label);
		connection_image_layout.addWidget(&connection_image);
		connection_image_layout.addWidget(&connection_browse);
		connection_group_layout.addWidget(&connection_image_row);

		QWidget connection_scene_row(&connection_group);
		QHBoxLayout connection_scene_layout(&connection_scene_row);
		connection_scene_layout.setContentsMargins(24, 0, 0, 0);
		QLabel connection_scene_label("Scene:", &connection_scene_row);
		connection_scene_label.setMinimumWidth(64);
		QComboBox connection_scene(&connection_scene_row);
		populate_scene_combo(connection_scene, current.connection.scene);
		connection_scene_layout.addWidget(&connection_scene_label);
		connection_scene_layout.addWidget(&connection_scene);
		connection_group_layout.addWidget(&connection_scene_row);

		layout.addWidget(&sensitive_group);
		layout.addWidget(&connection_group);

		QLabel image_note("Custom images are rendered after the complete OBS output over an opaque "
						  "black background. Missing or invalid images use the bundled fallback.",
			&dialog);
		image_note.setWordWrap(true);
		layout.addWidget(&image_note);

		QLabel warning("Custom scenes and transitions may expose captured content. Privacy "
					   "Guard cannot validate user-created scene contents or transition safety. "
					   "If a selected scene is unavailable, the bundled fallback is used.",
			&dialog);
		warning.setWordWrap(true);
		warning.setStyleSheet("color: #d9a441;");
		layout.addWidget(&warning);

		QCheckBox risk_accepted("I understand that I am responsible for testing my custom scenes and "
								"transitions before streaming.",
			&dialog);
		risk_accepted.setChecked(current.custom_scene_risk_accepted);
		layout.addWidget(&risk_accepted);

		QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
		layout.addWidget(&buttons);

		const auto refresh_controls = [&] {
			const DisplayMode sensitive_selection = selected_display_mode(sensitive_mode);
			const DisplayMode connection_selection = selected_display_mode(connection_mode);

			sensitive_image_row.setVisible(sensitive_selection == DisplayMode::CustomImage);
			sensitive_scene_row.setVisible(sensitive_selection == DisplayMode::CustomScene);
			connection_image_row.setVisible(connection_selection == DisplayMode::CustomImage);
			connection_scene_row.setVisible(connection_selection == DisplayMode::CustomScene);

			const bool any_custom_image = sensitive_selection == DisplayMode::CustomImage || connection_selection == DisplayMode::CustomImage;
			image_note.setVisible(any_custom_image);

			const bool any_custom_scene = sensitive_selection == DisplayMode::CustomScene || connection_selection == DisplayMode::CustomScene;
			warning.setVisible(any_custom_scene);
			risk_accepted.setVisible(any_custom_scene);
		};

		const auto choose_image = [&](QLineEdit &field) {
			const QString selected = QFileDialog::getOpenFileName(&dialog, "Select privacy image", field.text(), "Images (*.png *.jpg *.jpeg *.bmp *.webp);;All files (*)");
			if (!selected.isEmpty()) {
				field.setText(selected);
			}
		};

		QObject::connect(&sensitive_mode, &QComboBox::currentIndexChanged, &dialog, [&](int) { refresh_controls(); });
		QObject::connect(&connection_mode, &QComboBox::currentIndexChanged, &dialog, [&](int) { refresh_controls(); });
		QObject::connect(&sensitive_browse, &QPushButton::clicked, &dialog, [&] { choose_image(sensitive_image); });
		QObject::connect(&connection_browse, &QPushButton::clicked, &dialog, [&] { choose_image(connection_image); });
		QObject::connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
		QObject::connect(buttons.button(QDialogButtonBox::Ok), &QPushButton::clicked, &dialog, [&] {
			const DisplayMode sensitive_selection = selected_display_mode(sensitive_mode);
			const DisplayMode connection_selection = selected_display_mode(connection_mode);
			const bool any_custom_scene = sensitive_selection == DisplayMode::CustomScene || connection_selection == DisplayMode::CustomScene;
			if (any_custom_scene && !risk_accepted.isChecked()) {
				QMessageBox::warning(&dialog, "Risk acknowledgment required",
					"Accept responsibility for custom scenes and "
					"transitions, or select a post-render image option.");
				return;
			}

			if ((sensitive_selection == DisplayMode::CustomImage && load_privacy_image(sensitive_image.text()).isNull()) ||
				(connection_selection == DisplayMode::CustomImage && load_privacy_image(connection_image.text()).isNull())) {
				QMessageBox::warning(&dialog, "Image unavailable", "Select a readable image file for every custom-image event.");
				return;
			}

			const std::string sensitive_name = sensitive_scene.currentData().toString().toStdString();
			const std::string connection_name = connection_scene.currentData().toString().toStdString();
			if ((sensitive_selection == DisplayMode::CustomScene && !scene_exists(sensitive_name)) ||
				(connection_selection == DisplayMode::CustomScene && !scene_exists(connection_name))) {
				QMessageBox::warning(&dialog, "Scene unavailable",
					"Select an existing OBS scene for every enabled "
					"custom-scene event.");
				return;
			}

			if (privacy_state_.enabled() && !protection_enabled.isChecked()) {
				const QMessageBox::StandardButton choice = QMessageBox::warning(
					&dialog, "Disable Privacy Guard?",
					"Disabling Privacy Guard allows the complete OBS output to remain visible, "
					"even when VS Code reports a sensitive file or loses connection.\n\n"
					"Protection will automatically be enabled again the next time OBS starts.",
					QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);

				if (choice != QMessageBox::Yes) {
					return;
				}
			}

			dialog.accept();
		});

		refresh_controls();
		if (dialog.exec() != QDialog::Accepted) {
			return;
		}

		DisplaySettings updated;
		updated.sensitive.mode = selected_display_mode(sensitive_mode);
		updated.sensitive.image_path = sensitive_image.text().toStdString();
		updated.sensitive.scene = sensitive_scene.currentData().toString().toStdString();
		updated.connection.mode = selected_display_mode(connection_mode);
		updated.connection.image_path = connection_image.text().toStdString();
		updated.connection.scene = connection_scene.currentData().toString().toStdString();
		updated.custom_scene_risk_accepted = risk_accepted.isChecked();
		update_settings(std::move(updated));
		privacy_state_.set_enabled(protection_enabled.isChecked());
		obs_frontend_save();
	}

	void ProtectionDisplayController::save_load(obs_data *const save_data, const bool saving) {
		// OBS invokes this callback when the active scene collection is saved or loaded.
		if (saving) {
			const DisplaySettings current = settings_snapshot();
			obs_data_t *const data = obs_data_create();
			obs_data_set_int(data, sensitive_mode_key, static_cast<long long>(current.sensitive.mode));
			obs_data_set_string(data, sensitive_image_key, current.sensitive.image_path.c_str());
			obs_data_set_string(data, sensitive_scene_key, current.sensitive.scene.c_str());
			obs_data_set_int(data, connection_mode_key, static_cast<long long>(current.connection.mode));
			obs_data_set_string(data, connection_image_key, current.connection.image_path.c_str());
			obs_data_set_string(data, connection_scene_key, current.connection.scene.c_str());
			obs_data_set_bool(data, risk_accepted_key, current.custom_scene_risk_accepted);
			obs_data_set_obj(save_data, settings_key, data);
			obs_data_release(data);
			return;
		}

		obs_data_t *const data = obs_data_get_obj(save_data, settings_key);
		if (data == nullptr) {
			update_settings({});
			return;
		}

		DisplaySettings loaded;
		loaded.sensitive.mode = load_display_mode(data, sensitive_mode_key, legacy_use_sensitive_scene_key);
		const char *const sensitive_image = obs_data_get_string(data, sensitive_image_key);
		loaded.sensitive.image_path = sensitive_image != nullptr ? sensitive_image : "";
		const char *const sensitive_scene = obs_data_get_string(data, sensitive_scene_key);
		loaded.sensitive.scene = sensitive_scene != nullptr ? sensitive_scene : "";
		loaded.connection.mode = load_display_mode(data, connection_mode_key, legacy_use_connection_scene_key);
		const char *const connection_image = obs_data_get_string(data, connection_image_key);
		loaded.connection.image_path = connection_image != nullptr ? connection_image : "";
		const char *const connection_scene = obs_data_get_string(data, connection_scene_key);
		loaded.connection.scene = connection_scene != nullptr ? connection_scene : "";
		loaded.custom_scene_risk_accepted = obs_data_get_bool(data, risk_accepted_key);
		obs_data_release(data);
		update_settings(std::move(loaded));
	}

	void ProtectionDisplayController::frontend_event(const obs_frontend_event event) {
		// Collection and scene changes invalidate any cached scene bookkeeping.
		if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING) {
			Mode mode = Mode::AwaitingConnection;
			{
				const std::scoped_lock lock(mutex_);
				mode = latest_mode_;
			}
			watermark_required_ = false;
			overlay_required_ = is_protected_mode(mode);
			image_renderer_->clear();
			custom_scene_active_ = false;
			previous_scene_.clear();
			active_target_scene_.clear();
			return;
		}

		if (event == OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED && !active_target_scene_.empty() && !scene_exists(active_target_scene_)) {
			Mode mode = Mode::AwaitingConnection;
			{
				const std::scoped_lock lock(mutex_);
				mode = latest_mode_;
			}
			watermark_required_ = false;
			overlay_required_ = true;
			image_renderer_->select_default(mode);
			restore_previous_scene();
			blog(LOG_WARNING, "[OBS VS Code Privacy Guard] Active custom scene was removed; using "
							  "the bundled fallback");
		}
	}

	void ProtectionDisplayController::update_settings(DisplaySettings settings) {
		// Re-apply the current privacy mode immediately after a settings change.
		Mode mode = Mode::AwaitingConnection;
		{
			const std::scoped_lock lock(mutex_);
			settings_ = std::move(settings);
			mode = latest_mode_;
		}
		if (is_protected_mode(mode)) {
			watermark_required_ = false;
			overlay_required_ = true;
			image_renderer_->clear();
		}
		queue_apply(mode);
	}

	DisplaySettings ProtectionDisplayController::settings_snapshot() const {
		const std::scoped_lock lock(mutex_);
		return settings_;
	}

	void ProtectionDisplayController::shutdown() {
		// Drain queued UI tasks before allowing the controller to be destroyed.
		if (!alive_.exchange(false)) {
			return;
		}
		obs_queue_task(OBS_TASK_UI, barrier_task, nullptr, true);
		image_renderer_->shutdown();
		watermark_renderer_->shutdown();
	}

} // namespace privacy_guard
