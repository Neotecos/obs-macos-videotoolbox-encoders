cmake_minimum_required(VERSION 3.28...3.30)

set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum supported macOS version")
set(PLUGIN_MIN_OBS_VERSION "31.1.0")

# The visible version is a release decision. The internal version is the macOS
# installer receipt version and must be a monotonically increasing integer.
set(PLUGIN_VISIBLE_VERSION "1.0.0")
set(PLUGIN_INTERNAL_VERSION "2")

project(obs-macos-videotoolbox-encoders VERSION ${PLUGIN_VISIBLE_VERSION} LANGUAGES C CXX)

include(CTest)

set(OBS_SOURCE_DIR "" CACHE PATH "Path to the official OBS Studio ${PLUGIN_MIN_OBS_VERSION} (or newer) source tree")
set(OBS_BUILD_DIR "" CACHE PATH "Path to the official OBS Studio ${PLUGIN_MIN_OBS_VERSION} (or newer) build tree")
set(OBS_APP_BUNDLE "" CACHE PATH "Path to the official OBS Studio ${PLUGIN_MIN_OBS_VERSION} (or newer) OBS.app bundle")

if(NOT APPLE)
  message(FATAL_ERROR "obs-macos-videotoolbox-encoders only supports macOS")
endif()

if(NOT OBS_SOURCE_DIR OR NOT OBS_BUILD_DIR)
  message(FATAL_ERROR "Set OBS_SOURCE_DIR and OBS_BUILD_DIR to an official OBS Studio ${PLUGIN_MIN_OBS_VERSION} or newer checkout and build")
endif()

if(NOT OBS_APP_BUNDLE)
  set(OBS_APP_BUNDLE "${OBS_BUILD_DIR}/frontend/Release/OBS.app")
endif()

list(APPEND CMAKE_MODULE_PATH "${OBS_SOURCE_DIR}/cmake/finders")
load_cache("${OBS_BUILD_DIR}" READ_WITH_PREFIX OBS_BUILD_ CMAKE_PREFIX_PATH CMAKE_PROJECT_VERSION)
if(NOT OBS_BUILD_CMAKE_PROJECT_VERSION)
  message(FATAL_ERROR "Could not determine the OBS Studio version from ${OBS_BUILD_DIR}/CMakeCache.txt")
endif()
if(OBS_BUILD_CMAKE_PROJECT_VERSION VERSION_LESS PLUGIN_MIN_OBS_VERSION)
  message(FATAL_ERROR
          "OBS Studio ${OBS_BUILD_CMAKE_PROJECT_VERSION} is too old; ${PLUGIN_MIN_OBS_VERSION} or newer is required")
endif()
list(PREPEND CMAKE_PREFIX_PATH ${OBS_BUILD_CMAKE_PREFIX_PATH})
find_package(libobs CONFIG REQUIRED PATHS "${OBS_BUILD_DIR}/libobs" NO_DEFAULT_PATH)
find_package(obs-frontend-api CONFIG REQUIRED PATHS "${OBS_BUILD_DIR}/frontend/api" NO_DEFAULT_PATH)
find_package(Qt6 REQUIRED Widgets)

# OBS 31.1.0's Qt bundle exposes the retired AGL framework transitively. The
# plugin does not use it, so omit that link item to keep the documented minimum
# buildable with current macOS SDKs, where AGL is unavailable.
if(APPLE AND TARGET WrapOpenGL::WrapOpenGL)
  get_target_property(qt_opengl_link_libraries WrapOpenGL::WrapOpenGL INTERFACE_LINK_LIBRARIES)
  if(qt_opengl_link_libraries)
    list(REMOVE_ITEM qt_opengl_link_libraries "${WrapOpenGL_AGL}" "-framework AGL")
    set_property(TARGET WrapOpenGL::WrapOpenGL PROPERTY INTERFACE_LINK_LIBRARIES "${qt_opengl_link_libraries}")
  endif()
endif()

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/packaging/Distribution.xml.in"
  "${CMAKE_CURRENT_BINARY_DIR}/Distribution.xml"
  @ONLY
)

add_library(
  obs-macos-videotoolbox-encoders MODULE
  src/encoder.c
  src/enhanced-broadcasting.cpp
  src/enhanced-broadcasting.hpp
)

file(GLOB locale_files CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/data/locale/*.ini")
set_source_files_properties(${locale_files} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/locale")
target_sources(obs-macos-videotoolbox-encoders PRIVATE ${locale_files})

target_compile_features(obs-macos-videotoolbox-encoders PRIVATE c_std_17 cxx_std_17)
target_compile_definitions(obs-macos-videotoolbox-encoders PRIVATE ENABLE_HEVC)
target_link_libraries(
  obs-macos-videotoolbox-encoders
  PRIVATE
    OBS::libobs
    OBS::obs-frontend-api
    Qt::Widgets
    "$<LINK_LIBRARY:FRAMEWORK,AVFoundation.framework>"
    "$<LINK_LIBRARY:FRAMEWORK,Cocoa.framework>"
    "$<LINK_LIBRARY:FRAMEWORK,CoreFoundation.framework>"
    "$<LINK_LIBRARY:FRAMEWORK,CoreMedia.framework>"
    "$<LINK_LIBRARY:FRAMEWORK,CoreVideo.framework>"
    "$<LINK_LIBRARY:FRAMEWORK,VideoToolbox.framework>"
)

set_target_properties(
  obs-macos-videotoolbox-encoders
  PROPERTIES
    PREFIX ""
    OUTPUT_NAME "obs-macos-videotoolbox-encoders"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    BUNDLE TRUE
    BUNDLE_EXTENSION "plugin"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Info.plist.in"
    MACOSX_BUNDLE_BUNDLE_NAME "obs-macos-videotoolbox-encoders"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.alekstyle.obs.macos-videotoolbox-encoders"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PLUGIN_INTERNAL_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PLUGIN_VISIBLE_VERSION}"
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "@executable_path/../Frameworks"
    C_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
)

option(ENABLE_ADHOC_CODESIGN "Ad-hoc sign the built plugin bundle" ON)
if(ENABLE_ADHOC_CODESIGN)
  find_program(CODESIGN_EXECUTABLE codesign REQUIRED)
  add_custom_target(
    obs-macos-videotoolbox-encoders-codesign ALL
    COMMAND "${CODESIGN_EXECUTABLE}" --force --sign - "$<TARGET_BUNDLE_DIR:obs-macos-videotoolbox-encoders>"
    DEPENDS obs-macos-videotoolbox-encoders
    COMMENT "Ad-hoc signing obs-macos-videotoolbox-encoders.plugin"
    VERBATIM
  )
endif()

set(pkg_dependency obs-macos-videotoolbox-encoders)
if(ENABLE_ADHOC_CODESIGN)
  set(pkg_dependency obs-macos-videotoolbox-encoders-codesign)
endif()
add_custom_target(
  package-pkg
  COMMAND
    "${CMAKE_CURRENT_SOURCE_DIR}/packaging/build-pkg.sh"
    "$<TARGET_BUNDLE_DIR:obs-macos-videotoolbox-encoders>"
    "${CMAKE_CURRENT_SOURCE_DIR}/dist"
    "${CMAKE_CURRENT_BINARY_DIR}/Distribution.xml"
    "${PLUGIN_VISIBLE_VERSION}"
    "${PLUGIN_INTERNAL_VERSION}"
  DEPENDS ${pkg_dependency}
  COMMENT "Building macOS installer package"
  VERBATIM
)

install(
  TARGETS obs-macos-videotoolbox-encoders
  BUNDLE DESTINATION "."
  LIBRARY DESTINATION "."
)

if(BUILD_TESTING)
  add_executable(module-smoke-test tests/module-smoke-test.c)
  target_link_libraries(module-smoke-test PRIVATE OBS::libobs)

  set(obs_builtin_vt_bundle "${OBS_APP_BUNDLE}/Contents/PlugIns/mac-videotoolbox.plugin/Contents")
  add_test(
    NAME module-smoke-test
    COMMAND
      module-smoke-test
      "${obs_builtin_vt_bundle}/MacOS/mac-videotoolbox"
      "${obs_builtin_vt_bundle}/Resources"
      "$<TARGET_FILE:obs-macos-videotoolbox-encoders>"
      "$<TARGET_BUNDLE_CONTENT_DIR:obs-macos-videotoolbox-encoders>/Resources"
  )
  set_tests_properties(
    module-smoke-test
    PROPERTIES ENVIRONMENT
               "DYLD_LIBRARY_PATH=${OBS_APP_BUNDLE}/Contents/Frameworks;DYLD_FRAMEWORK_PATH=${OBS_APP_BUNDLE}/Contents/Frameworks"
  )
endif()
