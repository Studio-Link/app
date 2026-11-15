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
standalone_extra=()
plugin_formats=""

if [ "$BUILD_TARGET" == "linux" ]; then
    plugin_formats="CLAP;VST3;LV2"
fi

if [ "$BUILD_OS" == "macos" ]; then
    plugin_formats="CLAP;VST3;AUV2"
    export MACOSX_DEPLOYMENT_TARGET=10.13
    if [ "$BUILD_TARGET" == "macos_arm64" ]; then
        cmake_extra="-DCMAKE_OSX_ARCHITECTURES=arm64"
    else
        cmake_extra="-DCMAKE_OSX_ARCHITECTURES=x86_64"
    fi
    # StudioLink.app is a Swift/AppKit app (macOS 11+), CMake builds Swift
    # only with Ninja. Not signed, see docs/release.md.
    # Built per architecture (the 3rdparty libs are), merged into a
    # universal app by dist/macos_universal.sh.
    standalone_extra=(-G Ninja -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0)
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

if [ "$BUILD_OS" == "macos" ]; then
    # own build dir per architecture (Swift target is fixed at configure)
    SL_VARIANT_DIR=standalone_$BUILD_TARGET \
        sl_cmake_variant standalone $cmake_extra "${standalone_extra[@]}"
else
    sl_cmake_variant standalone $cmake_extra
fi
# DAW plugins, output in plugin/plugins and onair/plugins
sl_cmake_variant plugin $cmake_extra "-DSL_PLUGIN_FORMATS=$plugin_formats"
sl_cmake_variant onair $cmake_extra "-DSL_PLUGIN_FORMATS=$plugin_formats"

if [ "$BUILD_OS" != "macos" ]; then
    cp -a standalone/studio-link-standalone .
fi

# Testing and collect artifacts
#-----------------------------------------------------------------------------

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

    cp -a studio-link-standalone-$version_tc.tar.gz $artifacts
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
        cp -a $zip_name.zip $artifacts
    done
fi

if [ "$BUILD_OS" == "macos" ]; then
    # single architecture app, input for dist/macos_universal.sh
    arch=${BUILD_TARGET#macos_}
    app=StudioLink.app
    rm -rf $app
    cp -a standalone_$BUILD_TARGET/$app .
    otool -L $app/Contents/MacOS/StudioLink
    # macOS 11 has no Swift concurrency runtime (not bundled)
    if otool -L $app/Contents/MacOS/StudioLink | grep -q libswift_Concurrency; then
        echo "StudioLink links libswift_Concurrency, would not start on macOS 11"
        exit 1
    fi
    lipo -archs $app/Contents/MacOS/StudioLink
    # not stripped/signed: the linker signature keeps it runnable on Apple
    # Silicon, strip + codesign for releases, see docs/release.md
    sl_macos_zip $app studio-link-standalone-$version_t-$arch.zip
    cp -a studio-link-standalone-$version_t-$arch.zip $artifacts
fi
