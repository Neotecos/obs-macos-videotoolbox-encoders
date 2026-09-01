#include "plugin-config.h"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/config-file.h>

#include <string.h>

#define VT_CONFIG_SECTION "VideoToolboxCustomizableEncoders"

#define VT_LOOKAHEAD_MIN 1
#define VT_LOOKAHEAD_MAX 32
#define VT_LOOKAHEAD_DEFAULT 8

static const char *codec_prefix(const char *codec)
{
	return (strcmp(codec, "hevc") == 0) ? "HEVC" : "H264";
}

static config_t *get_config_with_defaults(const char *codec_key_prefix)
{
	config_t *config = obs_frontend_get_app_config();
	if (!config) {
		return NULL;
	}

	char key[64];

	snprintf(key, sizeof(key), "%s.RealTime", codec_key_prefix);
	config_set_default_bool(config, VT_CONFIG_SECTION, key, false);

	snprintf(key, sizeof(key), "%s.PrioritizeSpeed", codec_key_prefix);
	config_set_default_bool(config, VT_CONFIG_SECTION, key, false);

	snprintf(key, sizeof(key), "%s.LookaheadEnabled", codec_key_prefix);
	config_set_default_bool(config, VT_CONFIG_SECTION, key, false);

	snprintf(key, sizeof(key), "%s.LookaheadFrames", codec_key_prefix);
	config_set_default_int(config, VT_CONFIG_SECTION, key, VT_LOOKAHEAD_DEFAULT);

	config_set_default_bool(config, VT_CONFIG_SECTION, "Multitrack.UseCustomizableEncoders", false);

	return config;
}

static void save_config(config_t *config)
{
	if (config_save_safe(config, "tmp", NULL) != CONFIG_SUCCESS) {
		blog(LOG_WARNING, "[VideoToolbox encoder] Could not save the plugin configuration");
	}
}

void plugin_config_get_tuning(const char *codec, vt_codec_tuning_t *out)
{
	memset(out, 0, sizeof(*out));

	const char *prefix = codec_prefix(codec);
	config_t *config = get_config_with_defaults(prefix);
	if (!config) {
		out->lookahead_frames = VT_LOOKAHEAD_DEFAULT;
		return;
	}

	char key[64];

	snprintf(key, sizeof(key), "%s.RealTime", prefix);
	out->realtime = config_get_bool(config, VT_CONFIG_SECTION, key);

	snprintf(key, sizeof(key), "%s.PrioritizeSpeed", prefix);
	out->prioritize_speed = config_get_bool(config, VT_CONFIG_SECTION, key);

	snprintf(key, sizeof(key), "%s.LookaheadEnabled", prefix);
	out->lookahead_enabled = config_get_bool(config, VT_CONFIG_SECTION, key);

	snprintf(key, sizeof(key), "%s.LookaheadFrames", prefix);
	out->lookahead_frames = (int)config_get_int(config, VT_CONFIG_SECTION, key);
	if (out->lookahead_frames < VT_LOOKAHEAD_MIN) {
		out->lookahead_frames = VT_LOOKAHEAD_MIN;
	} else if (out->lookahead_frames > VT_LOOKAHEAD_MAX) {
		out->lookahead_frames = VT_LOOKAHEAD_MAX;
	}
}

void plugin_config_set_realtime(const char *codec, bool value)
{
	const char *prefix = codec_prefix(codec);
	config_t *config = get_config_with_defaults(prefix);
	if (!config) {
		return;
	}

	char key[64];
	snprintf(key, sizeof(key), "%s.RealTime", prefix);
	config_set_bool(config, VT_CONFIG_SECTION, key, value);
	save_config(config);
}

void plugin_config_set_prioritize_speed(const char *codec, bool value)
{
	const char *prefix = codec_prefix(codec);
	config_t *config = get_config_with_defaults(prefix);
	if (!config) {
		return;
	}

	char key[64];
	snprintf(key, sizeof(key), "%s.PrioritizeSpeed", prefix);
	config_set_bool(config, VT_CONFIG_SECTION, key, value);
	save_config(config);
}

void plugin_config_set_lookahead(const char *codec, bool enabled, int frames)
{
	const char *prefix = codec_prefix(codec);
	config_t *config = get_config_with_defaults(prefix);
	if (!config) {
		return;
	}

	if (frames < VT_LOOKAHEAD_MIN) {
		frames = VT_LOOKAHEAD_MIN;
	} else if (frames > VT_LOOKAHEAD_MAX) {
		frames = VT_LOOKAHEAD_MAX;
	}

	char key[64];
	snprintf(key, sizeof(key), "%s.LookaheadEnabled", prefix);
	config_set_bool(config, VT_CONFIG_SECTION, key, enabled);

	snprintf(key, sizeof(key), "%s.LookaheadFrames", prefix);
	config_set_int(config, VT_CONFIG_SECTION, key, frames);

	save_config(config);
}

bool plugin_config_get_multitrack_enabled(void)
{
	config_t *config = get_config_with_defaults("H264");
	return config && config_get_bool(config, VT_CONFIG_SECTION, "Multitrack.UseCustomizableEncoders");
}

void plugin_config_set_multitrack_enabled(bool value)
{
	config_t *config = get_config_with_defaults("H264");
	if (!config) {
		return;
	}

	config_set_bool(config, VT_CONFIG_SECTION, "Multitrack.UseCustomizableEncoders", value);
	save_config(config);
}

int plugin_config_lookahead_min(void)
{
	return VT_LOOKAHEAD_MIN;
}

int plugin_config_lookahead_max(void)
{
	return VT_LOOKAHEAD_MAX;
}

int plugin_config_lookahead_default(void)
{
	return VT_LOOKAHEAD_DEFAULT;
}
