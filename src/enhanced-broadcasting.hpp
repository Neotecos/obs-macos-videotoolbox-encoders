#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void multitrack_video_init(void);
void multitrack_video_shutdown(void);
void multitrack_video_add_encoder_mapping(const char *native_id, const char *plugin_id);
const char *multitrack_video_find_encoder_mapping(const char *native_id);

#ifdef __cplusplus
}
#endif
