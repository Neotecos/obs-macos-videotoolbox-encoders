cmake_minimum_required(VERSION 3.28...3.30)

project(mac-hw-encoders-videotoolbox VERSION 1.0.0 LANGUAGES C)

include(CTest)

set(OBS_SOURCE_DIR "/Users/alekstyle/CLionProjects/obs-studio" CACHE PATH "Path to the matching OBS Studio source tree")
set(OBS_BUILD_DIR "${OBS_SOURCE_DIR}/build_macos" CACHE PATH "Path to the matching OBS Studio build tree")

if(NOT APPLE)
  message(FATAL_ERROR "mac-hw-encoders-videotoolbox only supports macOS")
endif()

list(APPEND CMAKE_MODULE_PATH "${OBS_SOURCE_DIR}/cmake/finders")
load_cache("${OBS_BUILD_DIR}" READ_WITH_PREFIX OBS_BUILD_ CMAKE_PREFIX_PATH)
list(PREPEND CMAKE_PREFIX_PATH ${OBS_BUILD_CMAKE_PREFIX_PATH})
find_package(libobs CONFIG REQUIRED PATHS "${OBS_BUILD_DIR}/libobs" NO_DEFAULT_PATH)

add_library(mac-hw-encoders-videotoolbox MODULE src/encoder.c)

file(GLOB locale_files CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/data/locale/*.ini")
set_source_files_properties(${locale_files} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/locale")
target_sources(mac-hw-encoders-videotoolbox PRIVATE ${locale_files})

target_compile_features(mac-hw-encoders-videotoolbox PRIVATE c_std_17)
target_compile_definitions(mac-hw-encoders-videotoolbox PRIVATE ENABLE_HEVC)
target_link_libraries(
  mac-hw-encoders-videotoolbox
  PRIVATE
    OBS::libobs
    "$<LINK_LIBRARY:FRAMEWORK,AVFoundation.framework>"
    "$<LINK_LIBRARY:FRAMEWORK,Cocoa.framework>"
    "$<LINK_LIBRARY:FRAMEWORK,CoreFoundation.framework>"
    "$<LINK_LIBRARY:FRAMEWORK,CoreMedia.framework>"
    "$<LINK_LIBRARY:FRAMEWORK,CoreVideo.framework>"
    "$<LINK_LIBRARY:FRAMEWORK,VideoToolbox.framework>"
)

set_target_properties(
  mac-hw-encoders-videotoolbox
  PROPERTIES
    PREFIX ""
    OUTPUT_NAME "mac-hw-encoders-videotoolbox"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    BUNDLE TRUE
    BUNDLE_EXTENSION "plugin"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Info.plist.in"
    MACOSX_BUNDLE_BUNDLE_NAME "mac-hw-encoders-videotoolbox"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.alekstyle.obs.mac-hw-encoders-videotoolbox"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH ""
    C_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
)

option(ENABLE_ADHOC_CODESIGN "Ad-hoc sign the built plugin bundle" ON)
if(ENABLE_ADHOC_CODESIGN)
  find_program(CODESIGN_EXECUTABLE codesign REQUIRED)
  add_custom_target(
    mac-hw-encoders-videotoolbox-codesign ALL
    COMMAND "${CODESIGN_EXECUTABLE}" --force --sign - "$<TARGET_BUNDLE_DIR:mac-hw-encoders-videotoolbox>"
    DEPENDS mac-hw-encoders-videotoolbox
    COMMENT "Ad-hoc signing mac-hw-encoders-videotoolbox.plugin"
    VERBATIM
  )
endif()

set(pkg_dependency mac-hw-encoders-videotoolbox)
if(ENABLE_ADHOC_CODESIGN)
  set(pkg_dependency mac-hw-encoders-videotoolbox-codesign)
endif()
add_custom_target(
  package-pkg
  COMMAND
    "${CMAKE_CURRENT_SOURCE_DIR}/packaging/build-pkg.sh"
    "$<TARGET_BUNDLE_DIR:mac-hw-encoders-videotoolbox>"
    "${CMAKE_CURRENT_SOURCE_DIR}/dist"
  DEPENDS ${pkg_dependency}
  COMMENT "Building macOS installer package"
  VERBATIM
)

install(
  TARGETS mac-hw-encoders-videotoolbox
  BUNDLE DESTINATION "."
  LIBRARY DESTINATION "."
)

if(BUILD_TESTING)
  add_executable(module-smoke-test tests/module-smoke-test.c)
  target_link_libraries(module-smoke-test PRIVATE OBS::libobs)

  set(obs_builtin_vt_bundle "${OBS_BUILD_DIR}/OBS.app/Contents/PlugIns/mac-videotoolbox.plugin/Contents")
  add_test(
    NAME module-smoke-test
    COMMAND
      module-smoke-test
      "${obs_builtin_vt_bundle}/MacOS/mac-videotoolbox"
      "${obs_builtin_vt_bundle}/Resources"
      "$<TARGET_FILE:mac-hw-encoders-videotoolbox>"
      "$<TARGET_BUNDLE_CONTENT_DIR:mac-hw-encoders-videotoolbox>/Resources"
  )
  set_tests_properties(
    module-smoke-test
    PROPERTIES ENVIRONMENT "DYLD_LIBRARY_PATH=${OBS_BUILD_DIR}/OBS.app/Contents/Frameworks"
  )
endif()
