# mac-hw-encoders-videotoolbox

Standalone OBS Studio plugin providing Apple VideoToolbox hardware H.264 and
HEVC encoders with `kVTCompressionPropertyKey_RealTime` enabled.

The encoder implementation is derived from OBS Studio's `mac-videotoolbox`
module at tag `32.2.2-realtime.1`. Apart from separate OBS encoder IDs, display
names, and filtering the registration list to hardware H.264/HEVC entries, it
uses the same settings, defaults, VideoToolbox session configuration, frame
handling, and packet handling as that fork.

## Build

The default paths build against the matching local OBS fork:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Override `OBS_SOURCE_DIR` and `OBS_BUILD_DIR` when the matching OBS checkout or
build directory lives elsewhere. The OBS build must already contain a Release
build of `libobs`.

## Package and install

Stage the plugin bundle in `dist`:

```sh
cmake --install build --prefix ./dist
```

The resulting plugin bundle is:

- `mac-hw-encoders-videotoolbox.plugin`

Install that bundle at:

```text
~/Library/Application Support/obs-studio/plugins/mac-hw-encoders-videotoolbox.plugin
```

Alternatively, build the graphical macOS installer:

```sh
cmake --build build --target package-pkg
```

The generated `dist/mac-hw-encoders-videotoolbox-1.0.0-macos-arm64.pkg`
installs the plugin automatically for the currently logged-in user. It keeps a
system payload under `/Library/Application Support/Alekstyle/OBS Plugins` and
copies the plugin to the OBS user plugin directory in its post-install script.

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

Select **Apple VT H264 Hardware Encoder (RealTime)** or
**Apple VT HEVC Hardware Encoder (RealTime)** in OBS output settings.

The smoke test loads the original and standalone modules together, then checks
that every standalone encoder maps to the same Apple encoder ID and has the
same codec, capabilities, properties, and default settings as its original.

## License

GPL-2.0. The encoder implementation is derived from OBS Studio.
