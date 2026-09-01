#!/bin/sh

set -eu

script_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
project_dir="$(dirname "${script_dir}")"
plugin_bundle="${1:-${project_dir}/dist/obs-macos-videotoolbox-encoders.plugin}"
output_dir="${2:-${project_dir}/dist}"
distribution_file="${3:?CMake must provide a configured Distribution.xml}"
visible_version="${4:?CMake must provide the visible version}"
internal_version="${5:?CMake must provide the internal version}"
package_name="obs-macos-videotoolbox-encoders-${visible_version}-macos-arm64.pkg"
package_output="${output_dir}/${package_name}"
package_identifier="com.alekstyle.obs.macos-videotoolbox-encoders"
payload_location="Library/Application Support/Alekstyle/OBS Plugins"
work_dir="$(/usr/bin/mktemp -d /tmp/obs-macos-videotoolbox-encoders-pkg.XXXXXX)"

cleanup()
{
	/bin/rm -rf "${work_dir}"
}
trap cleanup EXIT INT TERM

if [ ! -d "${plugin_bundle}" ]; then
	echo "Plugin bundle not found: ${plugin_bundle}" >&2
	exit 1
fi

case "${internal_version}" in
	""|0|*[!0-9]*)
		echo "Internal version must be a positive integer: ${internal_version}" >&2
		exit 1
		;;
esac

/usr/bin/codesign --verify --deep --strict "${plugin_bundle}"

payload_root="${work_dir}/payload"
component_pkg="${work_dir}/obs-macos-videotoolbox-encoders-component.pkg"
resources_dir="${work_dir}/resources"
unsigned_product="${work_dir}/${package_name}"

/bin/mkdir -p "${payload_root}/${payload_location}" "${resources_dir}" "${output_dir}"
/usr/bin/ditto --norsrc --noextattr --noqtn --noacl \
	"${plugin_bundle}" \
	"${payload_root}/${payload_location}/obs-macos-videotoolbox-encoders.plugin"
/usr/bin/xattr -cr "${payload_root}"

if [ -n "${APP_SIGN_IDENTITY:-}" ]; then
	/usr/bin/codesign --force --sign "${APP_SIGN_IDENTITY}" --options runtime --timestamp \
		"${payload_root}/${payload_location}/obs-macos-videotoolbox-encoders.plugin"
fi

/usr/bin/codesign --verify --deep --strict \
	"${payload_root}/${payload_location}/obs-macos-videotoolbox-encoders.plugin"
/bin/cp "${project_dir}/LICENSE" "${resources_dir}/LICENSE"
/bin/cp "${script_dir}/resources/welcome.html" "${resources_dir}/welcome.html"

COPYFILE_DISABLE=1 /usr/bin/pkgbuild \
	--root "${payload_root}" \
	--component-plist "${script_dir}/component.plist" \
	--scripts "${script_dir}/scripts" \
	--identifier "${package_identifier}" \
	--version "${internal_version}" \
	--install-location "/" \
	--ownership recommended \
	"${component_pkg}"

COPYFILE_DISABLE=1 /usr/bin/productbuild \
	--distribution "${distribution_file}" \
	--resources "${resources_dir}" \
	--package-path "${work_dir}" \
	"${unsigned_product}"

if [ -n "${PKG_SIGN_IDENTITY:-}" ]; then
	/usr/bin/productsign --sign "${PKG_SIGN_IDENTITY}" "${unsigned_product}" "${package_output}.signed"
	/bin/mv -f "${package_output}.signed" "${package_output}"
else
	/bin/mv -f "${unsigned_product}" "${package_output}"
fi

if [ -n "${NOTARY_KEYCHAIN_PROFILE:-}" ]; then
	if [ -z "${APP_SIGN_IDENTITY:-}" ] || [ -z "${PKG_SIGN_IDENTITY:-}" ]; then
		echo "Notarization requires APP_SIGN_IDENTITY and PKG_SIGN_IDENTITY." >&2
		exit 1
	fi
	/usr/bin/xcrun notarytool submit "${package_output}" \
		--keychain-profile "${NOTARY_KEYCHAIN_PROFILE}" --wait
	/usr/bin/xcrun stapler staple "${package_output}"
	/usr/bin/xcrun stapler validate "${package_output}"
fi

/usr/sbin/pkgutil --check-signature "${package_output}" || true
/usr/bin/shasum -a 256 "${package_output}"
