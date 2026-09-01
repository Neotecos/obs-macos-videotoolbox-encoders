# obs-macos-videotoolbox-encoders

Standalone OBS Studio plugin that adds Apple VideoToolbox RealTime hardware
H.264 and HEVC encoders. It also lets OBS use those encoders for Multitrack
Video. The currently supported services are Twitch Enhanced Broadcasting,
Amazon IVS Multitrack Video, and Dolby OptiView Real-time Enhanced Broadcasting
(named Dolby Millicast in older OBS service lists).

This release targets the public API and build products of OBS Studio `31.1.0`
or newer. The plugin registers separate encoder IDs while retaining the native
Apple encoder IDs as the suffix, so each original VideoToolbox encoder has an
unambiguous RealTime counterpart.

The **Tools** menu includes **Use RealTime VideoToolbox encoders for Multitrack
Video**. When enabled, the option is stored in the current OBS profile. At
streaming start, it replaces only native Apple H.264/HEVC encoders created for
a supported Multitrack Video service with their matching RealTime counterparts.
Other streams and outputs are unchanged. Missing or incompatible replacements
fall back to the encoders selected by OBS.

## OBS compatibility

OBS Studio `31.1.0` is the supported minimum. The plugin uses only public
frontend and libobs APIs available in that release, including the APIs used by
the Multitrack override. Multitrack service support is detected at runtime from
the selected service configuration, so adding Twitch, Amazon IVS, or Dolby
OptiView support does not require raising the OBS minimum.

CMake reads the OBS version from the selected build cache and rejects any
version below `31.1.0`.

Do not raise this minimum without first building the plugin against the
official `31.1.0` tag and demonstrating that a required public API is missing.

## Build

Build OBS Studio `31.1.0` or newer first, then pass both paths explicitly. The
minimum-version validation should use the official `31.1.0` tag:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DOBS_SOURCE_DIR=/path/to/obs-studio-31.1.0 \
  -DOBS_BUILD_DIR=/path/to/obs-studio-31.1.0/build_macos
cmake --build build
ctest --test-dir build --output-on-failure
```

The OBS build must contain Release builds of OBS, `libobs`, and the public
frontend API.

## Package and install

Stage the plugin bundle in `dist`:

```sh
cmake --install build --prefix ./dist
```

The resulting plugin bundle is:

- `obs-macos-videotoolbox-encoders.plugin`

Install that bundle at:

```text
~/Library/Application Support/obs-studio/plugins/obs-macos-videotoolbox-encoders.plugin
```

Alternatively, build the graphical macOS installer:

```sh
cmake --build build --target package-pkg
```

The generated `dist/obs-macos-videotoolbox-encoders-1.0.0-macos-arm64.pkg`
installs the plugin automatically for the currently logged-in user. It keeps a
system payload under `/Library/Application Support/Alekstyle/OBS Plugins` and
copies the plugin to the OBS user plugin directory in its post-install script.

The visible release version and the monotonically increasing internal package
version are defined separately at the top of `CMakeLists.txt`. Update the
visible version manually for a release; increment the internal integer before
every build or PKG package so macOS applies the new payload.

Set `PKG_SIGN_IDENTITY` to a **Developer ID Installer** identity when producing
a signed release:

```sh
PKG_SIGN_IDENTITY='Developer ID Installer: Name (TEAMID)' \
  cmake --build build --target package-pkg
```

For a Gatekeeper-ready public release, sign the plugin with a **Developer ID
Application** identity and submit the signed PKG with a `notarytool` keychain
profile. The build waits for Apple, staples the ticket, and validates it:

```sh
APP_SIGN_IDENTITY='Developer ID Application: Name (TEAMID)' \
PKG_SIGN_IDENTITY='Developer ID Installer: Name (TEAMID)' \
NOTARY_KEYCHAIN_PROFILE='notary-profile' \
  cmake --build build --target package-pkg
```

Without notarization, the installer remains signed but Gatekeeper identifies it
as an unnotarized Developer ID package.

For ordinary outputs, select **Apple VT H264 Hardware Encoder (RealTime)** or
**Apple VT HEVC Hardware Encoder (RealTime)** directly in OBS output settings.

The smoke test loads the original and standalone modules together, then checks
that every standalone encoder maps to the same Apple encoder ID and has the
same codec, capabilities, properties, and default settings as its original.

## License

GPL-2.0. The encoder implementation is based on OBS Studio's GPL-licensed
VideoToolbox module.
