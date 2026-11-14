#
# Studio Link
#
# Builds libre, baresip (static, with the Studio Link modules compiled in)
# and the standalone application.
#
# Usage:
#   cmake -B build/standalone \
#         -DSL_VARIANT=standalone \
#         -DSL_3RDPARTY_DIR=build/3rdparty \
#         -DSL_RE_DIR=build/re -DSL_BARESIP_DIR=build/baresip
#   cmake --build build/standalone
#
# DAW plugins (SL_VARIANT plugin or onair), output in <build>/plugins:
#   -DSL_PLUGIN_FORMATS="CLAP;VST3;AUV2;LV2"
#
cmake_minimum_required(VERSION 3.18...4.0)

project(studiolink VERSION 26.11.0 LANGUAGES C)

set(SL_VARIANT "standalone" CACHE STRING "standalone, plugin or onair")
set_property(CACHE SL_VARIANT PROPERTY STRINGS standalone plugin onair)
set(SL_VERSION "v${PROJECT_VERSION}-dev" CACHE STRING "Version string")
set(SL_3RDPARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build/3rdparty"
  CACHE PATH "Prefix with the static 3rdparty libraries")
set(SL_RE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build/re"
  CACHE PATH "libre source directory")
set(SL_BARESIP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build/baresip"
  CACHE PATH "baresip source directory")

set(SL_PLUGIN_FORMATS "" CACHE STRING
  "DAW plugin formats to build: CLAP, VST3, AUV2 (macOS), LV2")
set(SL_CLAP_WRAPPER_VERSION "v0.17.0" CACHE STRING "clap-wrapper git tag")

# CA certificates, embedded (https://curl.se/ca/cacert-<version>.pem)
set(SL_CACERT_VERSION "2026-11-05" CACHE STRING "curl CA bundle date")
set(SL_CACERT_SHA256
  "427c064216761a1e72a8b30f0d954d08cb6e4435a82091fb865afd20c8497285"
  CACHE STRING "SHA256 of the curl CA bundle")
set(SL_CACERT_FILE "" CACHE FILEPATH
  "Local cacert.pem to embed (default: download SL_CACERT_VERSION)")

get_filename_component(SL_3RDPARTY_DIR "${SL_3RDPARTY_DIR}" ABSOLUTE)
list(PREPEND CMAKE_PREFIX_PATH "${SL_3RDPARTY_DIR}")
list(PREPEND CMAKE_FIND_ROOT_PATH "${SL_3RDPARTY_DIR}")

# Everything is linked statically, except the system audio/runtime libs
set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(OPENSSL_USE_STATIC_LIBS ON)
set(OPENSSL_ROOT_DIR "${SL_3RDPARTY_DIR}")
set(CMAKE_DISABLE_FIND_PACKAGE_ZLIB ON)

find_package(Threads REQUIRED)


##############################################################################
#
# 3rdparty libraries (static)
#

macro(sl_import_static name lib header)
  find_library(SL_${name}_LIBRARY NAMES ${lib}
    PATHS "${SL_3RDPARTY_DIR}/lib" NO_DEFAULT_PATH REQUIRED)
  find_path(SL_${name}_INCLUDE_DIR NAMES ${header}
    PATHS "${SL_3RDPARTY_DIR}/include" NO_DEFAULT_PATH REQUIRED)
  add_library(SL::${name} STATIC IMPORTED GLOBAL)
  set_target_properties(SL::${name} PROPERTIES
    IMPORTED_LOCATION "${SL_${name}_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${SL_${name}_INCLUDE_DIR}")
endmacro()

sl_import_static(FLAC FLAC FLAC/stream_encoder.h)
sl_import_static(samplerate samplerate samplerate.h)
sl_import_static(portaudio portaudio portaudio.h)
set_property(TARGET SL::FLAC APPEND PROPERTY
  INTERFACE_COMPILE_DEFINITIONS FLAC__NO_DLL)

# PortAudio host API dependencies
if(WIN32)
  set_property(TARGET SL::portaudio APPEND PROPERTY INTERFACE_LINK_LIBRARIES
    winmm ole32 uuid setupapi ksuser avrt)
elseif(APPLE)
  set_property(TARGET SL::portaudio APPEND PROPERTY INTERFACE_LINK_LIBRARIES
    "-framework CoreAudio" "-framework AudioToolbox"
    "-framework AudioUnit" "-framework CoreFoundation"
    "-framework CoreServices")
else()
  # these are runtime system libraries, so look for the shared ones
  set(_sl_suffixes ${CMAKE_FIND_LIBRARY_SUFFIXES})
  set(CMAKE_FIND_LIBRARY_SUFFIXES ".so")
  find_library(SL_PULSE_LIBRARY pulse)
  find_library(SL_JACK_LIBRARY jack)
  set(CMAKE_FIND_LIBRARY_SUFFIXES ${_sl_suffixes})

  set(_pa_deps Threads::Threads ${CMAKE_DL_LIBS} m)
  if(EXISTS "${SL_portaudio_INCLUDE_DIR}/pa_jack.h")
    if(NOT SL_JACK_LIBRARY)
      message(FATAL_ERROR "PortAudio was built with JACK, libjack missing")
    endif()
    list(APPEND _pa_deps ${SL_JACK_LIBRARY})
  endif()
  if(SL_PULSE_LIBRARY)
    list(APPEND _pa_deps ${SL_PULSE_LIBRARY})
  endif()
  set_property(TARGET SL::portaudio APPEND PROPERTY
    INTERFACE_LINK_LIBRARIES ${_pa_deps})
endif()


##############################################################################
#
# DAW plugins (before the directory wide compile definitions below)
#

if(SL_PLUGIN_FORMATS)
  if(SL_VARIANT STREQUAL "standalone")
    message(FATAL_ERROR "SL_PLUGIN_FORMATS needs SL_VARIANT plugin or onair")
  endif()
  set(_sl_formats ${SL_PLUGIN_FORMATS})
  list(REMOVE_ITEM _sl_formats LV2)
  if(_sl_formats)
    set(CMAKE_CXX_STANDARD 17)
    enable_language(CXX)
  endif()
  add_subdirectory(src/plugin)
endif()


##############################################################################
#
# Studio Link variant
#

# Common usage requirements of all Studio Link modules
add_library(sl_common INTERFACE)
target_include_directories(sl_common INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/webapp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/core)
target_compile_definitions(sl_common INTERFACE SLVERSION="${SL_VERSION}")

if(SL_VARIANT STREQUAL "standalone")
  set(SL_APP_MODULES webapp slaudio g722 slogging)
elseif(SL_VARIANT STREQUAL "plugin")
  set(SL_APP_MODULES webapp effect g722 slogging)
  add_compile_definitions(SLPLUGIN)
elseif(SL_VARIANT STREQUAL "onair")
  set(SL_APP_MODULES apponair effectonair g722 slogging)
  add_compile_definitions(SLIVE)
else()
  message(FATAL_ERROR "unknown SL_VARIANT: ${SL_VARIANT}")
endif()

if(WIN32)
  set(SL_UI_MODULE wincons)
else()
  set(SL_UI_MODULE stdio)
endif()


##############################################################################
#
# libre
#

set(LIBRE_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(LIBRE_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(USE_OPENSSL ON CACHE BOOL "" FORCE)
set(USE_BFCP OFF CACHE BOOL "" FORCE)
set(USE_RTMP OFF CACHE BOOL "" FORCE)
set(USE_PCP OFF CACHE BOOL "" FORCE)
add_subdirectory(${SL_RE_DIR} re EXCLUDE_FROM_ALL)

# Same libre feature definitions (HAVE_*, USE_*, OS/ARCH) for all SL code
find_package(re CONFIG REQUIRED HINTS ${SL_RE_DIR}/cmake NO_DEFAULT_PATH)
add_compile_definitions(${RE_DEFINITIONS})


##############################################################################
#
# CA certificates (curl/Mozilla bundle, embedded as sl_cacert_pem_data[])
#

if(SL_CACERT_FILE)
  set(_sl_cacert_pem "${SL_CACERT_FILE}")
else()
  set(_sl_cacert_pem "${CMAKE_BINARY_DIR}/cacert-${SL_CACERT_VERSION}.pem")
  if(NOT EXISTS "${_sl_cacert_pem}")
    file(DOWNLOAD "https://curl.se/ca/cacert-${SL_CACERT_VERSION}.pem"
      "${_sl_cacert_pem}.tmp"
      EXPECTED_HASH SHA256=${SL_CACERT_SHA256}
      TLS_VERIFY ON
      STATUS _sl_status)
    list(GET _sl_status 0 _sl_code)
    if(NOT _sl_code EQUAL 0)
      file(REMOVE "${_sl_cacert_pem}.tmp")
      message(FATAL_ERROR "cacert download failed: ${_sl_status}, "
        "set SL_CACERT_FILE for offline builds")
    endif()
    file(RENAME "${_sl_cacert_pem}.tmp" "${_sl_cacert_pem}")
  endif()
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${_sl_cacert_pem}")

file(READ "${_sl_cacert_pem}" _sl_hex HEX)
string(REPEAT "[0-9a-f][0-9a-f]" 16 _sl_line)
string(REGEX REPLACE "(${_sl_line})" "\\1\n" _sl_hex "${_sl_hex}")
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," _sl_hex "${_sl_hex}")
file(WRITE "${CMAKE_BINARY_DIR}/sl_cacert_data.c.tmp"
  "/* generated from ${_sl_cacert_pem}, do not edit */\n"
  "const char sl_cacert_pem_data[] = {\n${_sl_hex}0x00\n};\n")
configure_file("${CMAKE_BINARY_DIR}/sl_cacert_data.c.tmp"
  "${CMAKE_BINARY_DIR}/sl_cacert_data.c" COPYONLY)

add_library(slcacert STATIC
  src/core/slcacert.c src/core/slcacert.h
  "${CMAKE_BINARY_DIR}/sl_cacert_data.c")
target_include_directories(slcacert PUBLIC src/core)
target_link_libraries(slcacert PUBLIC re)
# all Studio Link modules can use it
target_link_libraries(sl_common INTERFACE slcacert)


##############################################################################
#
# baresip (static, Studio Link modules as APP_MODULES)
#

set(STATIC ON CACHE BOOL "" FORCE)
set(re_DIR "${SL_RE_DIR}/cmake" CACHE PATH "" FORCE)
set(RE_INCLUDE_DIR "${SL_RE_DIR}/include" CACHE PATH "" FORCE)
set(RE_LIBRARY re CACHE STRING "" FORCE)
set(MODULES ${SL_UI_MODULE} opus g711 ice stun turn dtls_srtp uuid auresamp
  CACHE STRING "" FORCE)
set(APP_MODULES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/modules"
  CACHE PATH "" FORCE)
set(APP_MODULES ${SL_APP_MODULES} CACHE STRING "" FORCE)
add_subdirectory(${SL_BARESIP_DIR} baresip EXCLUDE_FROM_ALL)


##############################################################################
#
# Studio Link core (configuration handling, shared by all variants)
#

add_library(slcore STATIC
  src/core/conf.c src/core/slconf.h
  src/core/slplugin.c src/core/slplugin.h)
target_include_directories(slcore PUBLIC src/core)
target_link_libraries(slcore PUBLIC baresip re sl_common)


##############################################################################
#
# Standalone executable
#

if(SL_VARIANT STREQUAL "standalone")
  add_executable(studio-link-standalone src/standalone/main.c)
  target_link_libraries(studio-link-standalone PRIVATE slcore baresip)
  if(UNIX AND NOT APPLE)
    # keep the binary portable: libgcc is linked statically, glibc stays
    # dynamic (for NSS/dlopen of libasound)
    target_link_options(studio-link-standalone PRIVATE -static-libgcc)
  endif()
  if(WIN32)
    target_link_options(studio-link-standalone PRIVATE -static)
  endif()
else()
  # plugin overlays link against libslcore.a + libbaresip.a
  add_custom_target(sl_libs ALL DEPENDS slcore baresip)
endif()

