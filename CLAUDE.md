# Release versioning

Before every compilation or PKG packaging, increase `PLUGIN_INTERNAL_VERSION`
in `CMakeLists.txt`. It is the macOS bundle and installer receipt version, must
always be a strictly increasing natural number, and must never contain dots.

`PLUGIN_VISIBLE_VERSION` is the user-facing release version. Change it only
when explicitly deciding the visible release number; it may contain dots and is
independent of the internal version.

Never reuse an internal version for a new PKG: macOS may retain the old payload
when the package identifier and internal version match an existing receipt.
