#
# macOS standalone: StudioLink.app (Swift/AppKit)
#
# The Swift app is the process entry point (main.swift), the core
# (src/standalone/main.c) runs on its own thread, see macapp.c. Included by
# the top-level CMakeLists.txt, defines the studio-link-standalone target.
#
# Needs the Ninja or Xcode generator (CMake has no Swift support for
# Makefiles):
#
#   cmake -G Ninja -B build/standalone -DSL_VARIANT=standalone ...
#
# The bundle is not signed (only the linker signature of the executable),
# release signing/notarization is done manually, see docs/release.md
#

set(SL_MAC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/standalone/macos")
# AppKit app for macOS 11+ (the plugins keep CMAKE_OSX_DEPLOYMENT_TARGET).
# No Swift concurrency in the app: its runtime is not part of macOS 11.
set(SL_MAC_DEPLOYMENT_TARGET "11.0")
set(SL_MAC_BUNDLE_ID "link.studio.standalone")
set(SL_MAC_EXECUTABLE "StudioLink")

if(NOT CMAKE_GENERATOR MATCHES "Ninja|Xcode")
  message(FATAL_ERROR "The macOS app is written in Swift, CMake supports "
    "Swift only with the Ninja or Xcode generator (cmake -G Ninja ...)")
endif()

list(LENGTH CMAKE_OSX_ARCHITECTURES _sl_narch)
if(_sl_narch GREATER 1)
  message(FATAL_ERROR "The macOS app is built per architecture, set "
    "CMAKE_OSX_ARCHITECTURES to arm64 or x86_64")
elseif(_sl_narch EQUAL 1)
  set(_sl_arch "${CMAKE_OSX_ARCHITECTURES}")
else()
  set(_sl_arch "${CMAKE_HOST_SYSTEM_PROCESSOR}")
endif()

if(NOT CMAKE_Swift_COMPILER_TARGET)
  set(CMAKE_Swift_COMPILER_TARGET
    "${_sl_arch}-apple-macosx${SL_MAC_DEPLOYMENT_TARGET}")
endif()
enable_language(Swift)


# App icon (.icns from the iconset, generated from icon/AppIcon.svg with
# dist/tools/macos_icons.sh)
set(_sl_icns "${CMAKE_CURRENT_BINARY_DIR}/StudioLink.icns")
file(GLOB _sl_icon_pngs "${SL_MAC_DIR}/StudioLink.iconset/*.png")
add_custom_command(OUTPUT "${_sl_icns}"
  COMMAND iconutil -c icns -o "${_sl_icns}" "${SL_MAC_DIR}/StudioLink.iconset"
  DEPENDS ${_sl_icon_pngs}
  COMMENT "Generating StudioLink.icns"
  VERBATIM)
# Menu bar icon (template image), see dist/tools/macos_icons.sh
set_source_files_properties("${_sl_icns}" "${SL_MAC_DIR}/MenuBarIcon.pdf"
  PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

configure_file("${SL_MAC_DIR}/Info.plist.in"
  "${CMAKE_CURRENT_BINARY_DIR}/Info.plist" @ONLY)


add_executable(studio-link-standalone MACOSX_BUNDLE
  src/standalone/main.c
  src/standalone/macapp.c
  src/standalone/macapp.h
  "${SL_MAC_DIR}/main.swift"
  "${SL_MAC_DIR}/AppDelegate.swift"
  "${SL_MAC_DIR}/MainMenu.swift"
  "${SL_MAC_DIR}/MenuBar.swift"
  "${SL_MAC_DIR}/Core.swift"
  "${SL_MAC_DIR}/MenuBarIcon.pdf"
  "${_sl_icns}")

set_target_properties(studio-link-standalone PROPERTIES
  OUTPUT_NAME "${SL_MAC_EXECUTABLE}"
  MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/Info.plist"
  LINKER_LANGUAGE Swift
  XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${SL_MAC_BUNDLE_ID}"
  XCODE_ATTRIBUTE_MACOSX_DEPLOYMENT_TARGET "${SL_MAC_DEPLOYMENT_TARGET}"
  XCODE_ATTRIBUTE_SWIFT_OBJC_BRIDGING_HEADER
    "${CMAKE_CURRENT_SOURCE_DIR}/src/standalone/macapp.h")

target_compile_definitions(studio-link-standalone PRIVATE
  $<$<COMPILE_LANGUAGE:C>:SL_MACAPP>)

# Swift 5 language mode: no strict concurrency checks/runtime isolation
# checks (Swift 6 mode would need the concurrency runtime)
target_compile_options(studio-link-standalone PRIVATE
  "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-swift-version 5>")

# Swift sees the C API through the bridging header (Xcode: attribute above)
if(NOT CMAKE_GENERATOR STREQUAL "Xcode")
  target_compile_options(studio-link-standalone PRIVATE
    "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-import-objc-header ${CMAKE_CURRENT_SOURCE_DIR}/src/standalone/macapp.h>")
endif()

target_link_libraries(studio-link-standalone PRIVATE
  "-framework AppKit" "-framework AVFoundation")
# drop unused Swift runtime dylibs (e.g. libswift_Concurrency, missing on
# macOS 11), dist/build.sh checks that it is not linked
target_link_options(studio-link-standalone PRIVATE
  "LINKER:-dead_strip_dylibs")

