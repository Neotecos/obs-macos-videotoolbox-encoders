#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	bool realtime;
	bool prioritize_speed;
	bool lookahead_enabled;
	int lookahead_frames;
} vt_codec_tuning_t;

/* codec must be "h264" or "hevc" */
void plugin_config_get_tuning(const char *codec, vt_codec_tuning_t *out);
void plugin_config_set_realtime(const char *codec, bool value);
void plugin_config_set_prioritize_speed(const char *codec, bool value);
void plugin_config_set_lookahead(const char *codec, bool enabled, int frames);

bool plugin_config_get_multitrack_enabled(void);
void plugin_config_set_multitrack_enabled(bool value);

int plugin_config_lookahead_min(void);
int plugin_config_lookahead_max(void);
int plugin_config_lookahead_default(void);

#ifdef __cplusplus
}
#endif
