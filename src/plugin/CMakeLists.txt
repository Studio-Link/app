#
# Studio Link DAW plugins (SL_VARIANT plugin or onair)
#
#   CLAP, VST3, AUV2  clap/sl_clap.c, VST3 and AUv2 via clap-wrapper
#   LV2               lv2/sl_lv2.c
#
# Added from the top level CMakeLists.txt before the directory wide compile
# definitions (libre OS/ARCH/HAVE_*), they break the VST3/AudioUnit SDKs.
# slcore and baresip are linked by name, CMake resolves them later.
#
# Output: <build>/plugins/
#

if(SL_VARIANT STREQUAL "plugin")
  set(_onair 0)
  set(_name StudioLink)
  set(_au_subtype Ypss)
  set(_lv2_name studio-link)
  set(_lv2_label "Studio Link")
else()
  set(_onair 1)
  set(_name StudioLinkOnAir)
  set(_au_subtype Eq2z)
  set(_lv2_name studio-link-onair)
  set(_lv2_label "Studio Link onAir")
endif()

set(SL_PLUGIN_OUTPUT_DIR "${CMAKE_BINARY_DIR}/plugins")

set(_clap_formats ${SL_PLUGIN_FORMATS})
list(REMOVE_ITEM _clap_formats LV2)

if(UNIX AND NOT APPLE)
  # keep the static libre/baresip/openssl symbols private, the host may
  # load its own (shared) openssl
  set(_sl_link_private "LINKER:--exclude-libs,ALL")
endif()


##############################################################################
#
# CLAP, VST3, AUv2
#

if(_clap_formats)
  include(FetchContent)

  # clap-wrapper downloads the CLAP, VST3 and AudioUnit SDKs (pinned by the
  # clap-wrapper version), unless CLAP_SDK_ROOT, VST3_SDK_ROOT or
  # AUDIOUNIT_SDK_ROOT are set
  set(CLAP_WRAPPER_DOWNLOAD_DEPENDENCIES ON CACHE BOOL "")
  FetchContent_Declare(clap-wrapper
    GIT_REPOSITORY https://github.com/free-audio/clap-wrapper.git
    GIT_TAG ${SL_CLAP_WRAPPER_VERSION}
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(clap-wrapper)

  add_library(sl_clap_impl STATIC clap/sl_clap.c)
  target_compile_definitions(sl_clap_impl PRIVATE
    SL_ONAIR=${_onair}
    SL_PLUGIN_VERSION="${PROJECT_VERSION}")
  target_link_libraries(sl_clap_impl PUBLIC clap slcore baresip)
  if(_sl_link_private)
    target_link_options(sl_clap_impl INTERFACE ${_sl_link_private})
  endif()

  make_clapfirst_plugins(
    TARGET_NAME ${_name}
    IMPL_TARGET sl_clap_impl
    OUTPUT_NAME ${_name}
    ENTRY_SOURCE clap/sl_clap_entry.cpp
    BUNDLE_IDENTIFIER com.itsr.${_name}
    BUNDLE_VERSION ${PROJECT_VERSION}
    COPY_AFTER_BUILD FALSE
    PLUGIN_FORMATS ${_clap_formats}
    ASSET_OUTPUT_DIRECTORY ${SL_PLUGIN_OUTPUT_DIR}

    # same AU identity as the old JUCE builds (existing DAW sessions)
    AUV2_MANUFACTURER_NAME ITSR
    AUV2_MANUFACTURER_CODE ITSR
    AUV2_SUBTYPE_CODE ${_au_subtype}
    AUV2_INSTRUMENT_TYPE aufx)
endif()


##############################################################################
#
# LV2
#

if(LV2 IN_LIST SL_PLUGIN_FORMATS)
  set(_lv2_dir "${SL_PLUGIN_OUTPUT_DIR}/${_lv2_name}.lv2")
  set(SL_LV2_URI "http://studio-link.de/plugins/lv2/${_lv2_name}")
  set(SL_LV2_NAME ${_lv2_name})
  set(SL_LV2_LABEL ${_lv2_label})
  set(SL_LV2_BINARY ${_lv2_name}${CMAKE_SHARED_MODULE_SUFFIX})

  add_library(sl_lv2 MODULE lv2/sl_lv2.c)
  target_compile_definitions(sl_lv2 PRIVATE SL_ONAIR=${_onair})
  target_link_libraries(sl_lv2 PRIVATE slcore baresip)
  if(_sl_link_private)
    target_link_options(sl_lv2 PRIVATE ${_sl_link_private})
  endif()
  set_target_properties(sl_lv2 PROPERTIES
    PREFIX ""
    OUTPUT_NAME ${_lv2_name}
    C_VISIBILITY_PRESET hidden
    LIBRARY_OUTPUT_DIRECTORY ${_lv2_dir})

  configure_file(lv2/manifest.ttl.in ${_lv2_dir}/manifest.ttl @ONLY)
  configure_file(lv2/plugin.ttl.in ${_lv2_dir}/${_lv2_name}.ttl @ONLY)
endif()
