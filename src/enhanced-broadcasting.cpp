#include "enhanced-broadcasting.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/config-file.h>

#include <QAction>
#include <QSignalBlocker>

#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr char kConfigSection[] = "MacHWEncodersVideoToolbox";
/* Retain the original key so existing profiles keep their preference. */
constexpr char kConfigKey[] = "EnhancedBroadcastingRealTime";
constexpr char kMultitrackOutputName[] = "rtmp multitrack video";
constexpr char kPluginEncoderPrefix[] = "obs-macos-videotoolbox-encoders.";
constexpr char kNativeVideoToolboxPrefix[] = "com.apple.videotoolbox.";

QAction *menu_action = nullptr;
obs_encoder_group_t *active_group = nullptr;
bool callback_registered = false;

struct EncoderSlot {
	size_t index = 0;
	obs_encoder_t *original = nullptr;
	obs_encoder_t *replacement = nullptr;
	std::string original_id;
	std::string selected_id;
	std::string codec;
};

void release_slots(std::vector<EncoderSlot> &encoder_slots)
{
	for (EncoderSlot &slot : encoder_slots) {
		obs_encoder_release(slot.replacement);
		obs_encoder_release(slot.original);
		slot.replacement = nullptr;
		slot.original = nullptr;
	}
}

bool option_enabled()
{
	config_t *config = obs_frontend_get_profile_config();
	return config && config_get_bool(config, kConfigSection, kConfigKey);
}

void set_option_default()
{
	config_t *config = obs_frontend_get_profile_config();
	if (config) {
		config_set_default_bool(config, kConfigSection, kConfigKey, false);
	}
}

void save_option(bool enabled)
{
	config_t *config = obs_frontend_get_profile_config();
	if (!config) {
		return;
	}

	config_set_bool(config, kConfigSection, kConfigKey, enabled);
	if (config_save_safe(config, "tmp", nullptr) != CONFIG_SUCCESS) {
		blog(LOG_WARNING, "[VideoToolbox encoder] Could not save the Multitrack Video preference");
	}
}

void refresh_action()
{
	if (!menu_action) {
		return;
	}

	set_option_default();
	const QSignalBlocker blocker(menu_action);
	menu_action->setChecked(option_enabled());
}

void destroy_active_group()
{
	if (!active_group) {
		return;
	}

	obs_encoder_group_destroy(active_group);
	active_group = nullptr;
}

bool is_supported_multitrack_output(obs_output_t *output)
{
	if (!output || std::strcmp(obs_output_get_id(output), "rtmp_output") != 0 ||
	    std::strcmp(obs_output_get_name(output), kMultitrackOutputName) != 0) {
		return false;
	}

	config_t *config = obs_frontend_get_profile_config();
	if (!config || !config_get_bool(config, "Stream1", "EnableMultitrackVideo")) {
		return false;
	}

	obs_service_t *service = obs_frontend_get_streaming_service();
	if (!service || std::strcmp(obs_service_get_id(service), "rtmp_common") != 0) {
		return false;
	}

	obs_data_t *settings = obs_service_get_settings(service);
	if (!settings) {
		return false;
	}

	const char *configuration_url = obs_data_get_string(settings, "multitrack_video_configuration_url");
	const bool supports_multitrack = configuration_url && configuration_url[0] != '\0';
	obs_data_release(settings);
	return supports_multitrack;
}

bool is_native_videotoolbox_encoder(const char *id, const char *codec)
{
	if (!id || !codec || std::strncmp(id, kNativeVideoToolboxPrefix, sizeof(kNativeVideoToolboxPrefix) - 1) != 0) {
		return false;
	}

	return std::strcmp(codec, "h264") == 0 || std::strcmp(codec, "hevc") == 0;
}

obs_encoder_t *create_replacement(obs_encoder_t *original, const std::string &target_id)
{
	const char *codec = obs_encoder_get_codec(original);
	const char *target_codec = obs_get_encoder_codec(target_id.c_str());
	if (!target_codec || std::strcmp(codec, target_codec) != 0) {
		return nullptr;
	}

	if (obs_encoder_get_caps(original) != obs_get_encoder_caps(target_id.c_str())) {
		return nullptr;
	}

	obs_data_t *settings = obs_encoder_get_settings(original);
	if (!settings) {
		return nullptr;
	}

	std::string name = obs_encoder_get_name(original);
	name += " (RealTime Multitrack)";
	obs_encoder_t *replacement = obs_video_encoder_create(target_id.c_str(), name.c_str(), settings, nullptr);
	obs_data_release(settings);
	if (!replacement) {
		return nullptr;
	}

	obs_encoder_set_video(replacement, obs_encoder_parent_video(original));
	if (obs_encoder_scaling_enabled(original)) {
		obs_encoder_set_scaled_size(replacement, obs_encoder_get_width(original),
					    obs_encoder_get_height(original));
		obs_encoder_set_gpu_scale_type(replacement, obs_encoder_get_scale_type(original));
	}
	obs_encoder_set_preferred_video_format(replacement, obs_encoder_get_preferred_video_format(original));
	obs_encoder_set_preferred_color_space(replacement, obs_encoder_get_preferred_color_space(original));
	obs_encoder_set_preferred_range(replacement, obs_encoder_get_preferred_range(original));

	if (!obs_encoder_set_frame_rate_divisor(replacement, obs_encoder_get_frame_rate_divisor(original))) {
		obs_encoder_release(replacement);
		return nullptr;
	}

	return replacement;
}

obs_encoder_group_t *group_original_encoders(const std::vector<EncoderSlot> &encoder_slots)
{
	obs_encoder_group_t *group = obs_encoder_group_create();
	if (!group) {
		return nullptr;
	}

	for (const EncoderSlot &slot : encoder_slots) {
		if (!obs_encoder_set_group(slot.original, group)) {
			obs_encoder_group_destroy(group);
			return nullptr;
		}
	}

	return group;
}

bool install_replacements(obs_output_t *output, const std::vector<EncoderSlot> &encoder_slots)
{
	obs_encoder_group_t *group = obs_encoder_group_create();
	if (!group) {
		return false;
	}

	for (const EncoderSlot &slot : encoder_slots) {
		if (!obs_encoder_set_group(slot.original, nullptr)) {
			obs_encoder_group_destroy(group);
			active_group = group_original_encoders(encoder_slots);
			return false;
		}
	}

	for (const EncoderSlot &slot : encoder_slots) {
		obs_encoder_t *selected = slot.replacement ? slot.replacement : slot.original;
		if (!obs_encoder_set_group(selected, group)) {
			obs_encoder_group_destroy(group);
			active_group = group_original_encoders(encoder_slots);
			return false;
		}
	}

	for (const EncoderSlot &slot : encoder_slots) {
		if (slot.replacement) {
			obs_output_set_video_encoder2(output, slot.replacement, slot.index);
		}
	}

	active_group = group;
	return true;
}

void apply_multitrack_video_override()
{
	destroy_active_group();
	if (!option_enabled()) {
		return;
	}

	obs_output_t *output = obs_frontend_get_streaming_output();
	if (!output) {
		return;
	}

	if (!is_supported_multitrack_output(output)) {
		obs_output_release(output);
		return;
	}

	config_t *config = obs_frontend_get_profile_config();
	if (config_get_bool(config, "Stream1", "MultitrackVideoStreamDumpEnabled")) {
		blog(LOG_WARNING, "[VideoToolbox encoder] Multitrack Video RealTime override "
				  "skipped while the multitrack stream dump is enabled");
		obs_output_release(output);
		return;
	}

	std::vector<EncoderSlot> encoder_slots;
	size_t replacement_count = 0;
	for (size_t index = 0; index < MAX_OUTPUT_VIDEO_ENCODERS; ++index) {
		obs_encoder_t *borrowed = obs_output_get_video_encoder2(output, index);
		obs_encoder_t *original = borrowed ? obs_encoder_get_ref(borrowed) : nullptr;
		if (!original) {
			continue;
		}

		EncoderSlot slot;
		slot.index = index;
		slot.original = original;
		slot.original_id = obs_encoder_get_id(original);
		slot.selected_id = slot.original_id;
		slot.codec = obs_encoder_get_codec(original);

		if (is_native_videotoolbox_encoder(slot.original_id.c_str(), slot.codec.c_str())) {
			const std::string target_id = std::string(kPluginEncoderPrefix) + slot.original_id;
			slot.replacement = create_replacement(original, target_id);
			if (slot.replacement) {
				slot.selected_id = target_id;
				++replacement_count;
			}
		}

		encoder_slots.emplace_back(std::move(slot));
	}

	bool installed = replacement_count > 0 && install_replacements(output, encoder_slots);
	if (!installed) {
		replacement_count = 0;
	}

	for (const EncoderSlot &slot : encoder_slots) {
		const bool overridden = installed && slot.replacement;
		blog(LOG_INFO,
		     "[VideoToolbox encoder] Multitrack Video index=%zu codec=%s "
		     "original=%s selected=%s override=%s",
		     slot.index, slot.codec.c_str(), slot.original_id.c_str(),
		     overridden ? slot.selected_id.c_str() : slot.original_id.c_str(), overridden ? "yes" : "no");
	}

	if (!installed) {
		blog(LOG_WARNING, "[VideoToolbox encoder] Multitrack Video RealTime override "
				  "unavailable; OBS will use its original encoders");
	} else {
		blog(LOG_INFO,
		     "[VideoToolbox encoder] Multitrack Video RealTime override "
		     "applied to %zu encoder(s)",
		     replacement_count);
	}

	release_slots(encoder_slots);
	obs_output_release(output);
}

void frontend_event(enum obs_frontend_event event, void *)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_STREAMING_STARTING:
		apply_multitrack_video_override();
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
		destroy_active_group();
		break;
	case OBS_FRONTEND_EVENT_PROFILE_CHANGED:
		refresh_action();
		break;
	case OBS_FRONTEND_EVENT_EXIT:
		destroy_active_group();
		break;
	default:
		break;
	}
}

} // namespace

void multitrack_video_init(void)
{
	menu_action = static_cast<QAction *>(
		obs_frontend_add_tools_menu_qaction(obs_module_text("MultitrackVideo.UseRealTime")));
	if (!menu_action) {
		blog(LOG_INFO, "[VideoToolbox encoder] OBS frontend unavailable; Multitrack Video integration disabled");
		return;
	}

	menu_action->setCheckable(true);
	refresh_action();
	QObject::connect(menu_action, &QAction::toggled, [](bool enabled) { save_option(enabled); });
	obs_frontend_add_event_callback(frontend_event, nullptr);
	callback_registered = true;
}

void multitrack_video_shutdown(void)
{
	if (callback_registered) {
		obs_frontend_remove_event_callback(frontend_event, nullptr);
		callback_registered = false;
	}
	destroy_active_group();
	menu_action = nullptr;
}
