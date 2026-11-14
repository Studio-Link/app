#!/bin/bash -ex
#
# BUILD_TARGET: linux | linuxjack | macos_x86_64 | macos_arm64 | ccheck
# BUILD_OS:     linux | macos
#
# Optional:
#   BUILD_TYPE=Debug       debug build (re type checked printf, mem debug)
#   SL_SKIP_PLUGINS=1      don't build the DAW plugins (CLAP/VST3/AUv2/LV2)
#   SL_SKIP_WEBUI=1        use the existing src/modules/webapp/assets
#

source dist/lib/versions.sh
source dist/lib/functions.sh

if [ "$BUILD_TARGET" == "ccheck" ]; then
    dist/tools/ccheck.sh
    exit 0
fi

# Start build
#-----------------------------------------------------------------------------
sl_prepare
sl_3rdparty

cmake_extra=""
plugin_formats=""

if [ "$BUILD_TARGET" == "linux" ]; then
    plugin_formats="CLAP;VST3;LV2"
fi

if [ "$BUILD_OS" == "macos" ]; then
    plugin_formats="CLAP;VST3;AUV2"
    export MACOSX_DEPLOYMENT_TARGET=10.13
    sed_opt="-i ''"
    if [ "$BUILD_TARGET" == "macos_arm64" ]; then
        cmake_extra="-DCMAKE_OSX_ARCHITECTURES=arm64"
    else
        cmake_extra="-DCMAKE_OSX_ARCHITECTURES=x86_64"
    fi
fi

# Sources
#-----------------------------------------------------------------------------
if [ -z "$SL_SKIP_WEBUI" ]; then
    sl_get_webui
fi

if [ ! -d re-$re ]; then
    sl_get_libre
fi

if [ ! -d baresip-$baresip ]; then
    sl_get_baresip
fi

# Build re + baresip + Studio Link (all static)
#-----------------------------------------------------------------------------
if [ -n "$SL_SKIP_PLUGINS" ]; then
    plugin_formats=""
fi

sl_cmake_variant standalone $cmake_extra
# DAW plugins, output in plugin/plugins and onair/plugins
sl_cmake_variant plugin $cmake_extra "-DSL_PLUGIN_FORMATS=$plugin_formats"
sl_cmake_variant onair $cmake_extra "-DSL_PLUGIN_FORMATS=$plugin_formats"

cp -a standalone/studio-link-standalone .

# Testing and prepare release upload
#-----------------------------------------------------------------------------
s3_path="s3_upload/$GIT_BRANCH/$version_t/$BUILD_TARGET"
mkdir -p $s3_path

if [ "$BUILD_TARGET" == "linux" ] || [ "$BUILD_TARGET" == "linuxjack" ]; then
    # only system/audio server libs should be dynamic
    # direct dependencies (ldd also lists libpulse's own dependencies)
    readelf -d studio-link-standalone | grep NEEDED
    if readelf -d studio-link-standalone | grep NEEDED | \
        grep -E "libssl|libcrypto|libopus|libFLAC|libsamplerate|libportaudio|libasound"; then
        echo "unexpected dynamic dependency"
        exit 1
    fi

    strip --strip-all studio-link-standalone

    chmod +x studio-link-standalone
    cp -a studio-link-standalone studio-link-standalone-$version_tc
    tar -czf studio-link-standalone-$version_tc.tar.gz \
        studio-link-standalone-$version_tc

    cp -a studio-link-standalone-$version_tc.tar.gz $s3_path
fi

# DAW plugins (one zip per variant with all formats)
#-----------------------------------------------------------------------------
if [ -n "$plugin_formats" ]; then
    for variant in plugin onair; do
        zip_name=studio-link-plugin
        [ "$variant" == "onair" ] && zip_name=studio-link-plugin-onair

        if [ "$BUILD_OS" == "linux" ]; then
            find $variant/plugins -type f -name "*.so" \
                -exec strip --strip-unneeded {} \;
            find $variant/plugins -type f -name "*.clap" \
                -exec strip --strip-unneeded {} \;
        fi

        rm -f $zip_name.zip
        (cd $variant/plugins && zip -qr ../../$zip_name.zip .)
        cp -a $zip_name.zip $s3_path
    done
fi

if [ "$BUILD_OS" == "macos" ]; then
    otool -L studio-link-standalone
    strip studio-link-standalone
    zip -r studio-link-standalone-$version_t.zip studio-link-standalone
    cp -a studio-link-standalone-$version_t.zip $s3_path
fi
