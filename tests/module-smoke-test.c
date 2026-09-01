#include <obs.h>

#include <stdio.h>
#include <string.h>

#include "plugin-ids.h"

static bool load_module(const char *binary_path, const char *data_path)
{
	obs_module_t *module = NULL;
	int result = obs_open_module(&module, binary_path, data_path);
	if (result != MODULE_SUCCESS) {
		fprintf(stderr, "obs_open_module failed for %s: %d\n", binary_path, result);
		return false;
	}

	if (!obs_init_module(module)) {
		fprintf(stderr, "obs_init_module failed for %s\n", binary_path);
		return false;
	}

	return true;
}

static bool properties_match(const char *upstream_id, const char *plugin_id)
{
	obs_properties_t *upstream_props = obs_get_encoder_properties(upstream_id);
	obs_properties_t *plugin_props = obs_get_encoder_properties(plugin_id);
	obs_property_t *upstream = obs_properties_first(upstream_props);
	obs_property_t *plugin = obs_properties_first(plugin_props);
	bool matches = true;

	while (upstream && plugin) {
		if (strcmp(obs_property_name(upstream), obs_property_name(plugin)) != 0 ||
		    obs_property_get_type(upstream) != obs_property_get_type(plugin)) {
			matches = false;
			break;
		}

		obs_property_next(&upstream);
		obs_property_next(&plugin);
	}

	if (upstream || plugin)
		matches = false;

	obs_properties_destroy(upstream_props);
	obs_properties_destroy(plugin_props);
	return matches;
}

static bool defaults_match(const char *upstream_id, const char *plugin_id)
{
	obs_data_t *upstream_defaults = obs_encoder_defaults(upstream_id);
	obs_data_t *plugin_defaults = obs_encoder_defaults(plugin_id);
	const char *upstream_json = obs_data_get_json(upstream_defaults);
	const char *plugin_json = obs_data_get_json(plugin_defaults);
	bool matches = strcmp(upstream_json, plugin_json) == 0;

	obs_data_release(upstream_defaults);
	obs_data_release(plugin_defaults);
	return matches;
}

int main(int argc, char **argv)
{
	if (argc != 5) {
		fprintf(stderr, "usage: %s <upstream-bin> <upstream-data> <plugin-bin> <plugin-data>\n", argv[0]);
		return 2;
	}

	if (!obs_startup("en-US", NULL, NULL)) {
		fprintf(stderr, "obs_startup failed\n");
		return 1;
	}

	if (!load_module(argv[1], argv[2]) || !load_module(argv[3], argv[4])) {
		obs_shutdown();
		return 1;
	}

	obs_post_load_modules();
	printf("testing against OBS %s (0x%08x)\n", obs_get_version_string(), obs_get_version());

	size_t plugin_encoder_count = 0;
	for (size_t index = 0;; index++) {
		const char *plugin_id = NULL;
		if (!obs_enum_encoder_types(index, &plugin_id))
			break;
		if (strncmp(plugin_id, OBS_MACOS_VT_ENCODER_ID_PREFIX, sizeof(OBS_MACOS_VT_ENCODER_ID_PREFIX) - 1) != 0)
			continue;

		const char *upstream_id = plugin_id + sizeof(OBS_MACOS_VT_ENCODER_ID_PREFIX) - 1;
		const char *upstream_codec = obs_get_encoder_codec(upstream_id);
		const char *plugin_codec = obs_get_encoder_codec(plugin_id);
		const char *display_name = obs_encoder_get_display_name(plugin_id);

		if (!upstream_codec || !plugin_codec || strcmp(upstream_codec, plugin_codec) != 0) {
			fprintf(stderr, "codec mismatch for %s\n", plugin_id);
			obs_shutdown();
			return 1;
		}
		if (strcmp(plugin_codec, "h264") != 0 && strcmp(plugin_codec, "hevc") != 0) {
			fprintf(stderr, "unexpected codec for %s: %s\n", plugin_id, plugin_codec);
			obs_shutdown();
			return 1;
		}
		if (!display_name || !strstr(display_name, "(RealTime)")) {
			fprintf(stderr, "unexpected display name for %s\n", plugin_id);
			obs_shutdown();
			return 1;
		}
		const uint32_t upstream_caps = obs_get_encoder_caps(upstream_id);
		const uint32_t plugin_caps = obs_get_encoder_caps(plugin_id);
		if (upstream_caps != plugin_caps) {
			fprintf(stderr, "capability mismatch for %s: upstream=0x%08x plugin=0x%08x\n", plugin_id,
				upstream_caps, plugin_caps);
			obs_shutdown();
			return 1;
		}
		if (!properties_match(upstream_id, plugin_id)) {
			fprintf(stderr, "property mismatch for %s\n", plugin_id);
			obs_shutdown();
			return 1;
		}
		if (!defaults_match(upstream_id, plugin_id)) {
			fprintf(stderr, "default mismatch for %s\n", plugin_id);
			obs_shutdown();
			return 1;
		}

		printf("verified %s (%s) against %s with capabilities 0x%08x\n", plugin_id, display_name, upstream_id,
		       plugin_caps);
		plugin_encoder_count++;
	}

	obs_shutdown();
	if (plugin_encoder_count < 2) {
		fprintf(stderr, "expected at least H.264 and HEVC hardware encoders, found %zu\n", plugin_encoder_count);
		return 1;
	}

	return 0;
}
