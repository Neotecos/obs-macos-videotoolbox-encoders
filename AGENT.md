# Release versioning

Before every compilation or PKG packaging, increase `PLUGIN_INTERNAL_VERSION`
in `CMakeLists.txt`. It is the macOS bundle and installer receipt version, must
always be a strictly increasing natural number, and must never contain dots.

`PLUGIN_VISIBLE_VERSION` is the user-facing release version. Change it only
when explicitly deciding the visible release number; it may contain dots and is
independent of the internal version.

Never reuse an internal version for a new PKG: macOS may retain the old payload
when the package identifier and internal version match an existing receipt.

# OBS compatibility baseline

`31.1.0` is the minimum supported OBS Studio version. It remains the baseline
after the Multitrack integrations because the plugin uses public frontend and
libobs APIs already present in that release; services are detected at runtime.
CMake enforces this minimum from the selected OBS build cache.

Before increasing this minimum, build the plugin against the official `31.1.0`
tag and document the exact required public API that is unavailable there.

All current and future changes must remain compatible across the complete
supported OBS Studio range, from `PLUGIN_MIN_OBS_VERSION` through the latest
stable release. Implementations, build logic, and tests must not assume only
the compile-time OBS version or only the newest runtime. When APIs, capabilities,
or behavior differ by OBS version, use a backward-compatible implementation and
verify at least the oldest supported version and the latest stable release.
