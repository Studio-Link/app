#!/bin/bash -ex
#
# Cross build for Windows with mingw-w64 (BUILD_TARGET: windows64)
#
# Optional:
#   SL_SKIP_PLUGINS=1      don't build the DAW plugins (CLAP/VST3)
#   SL_SKIP_WEBUI=1        use the existing src/modules/webapp/assets
#
# NOTE: not tested yet after the baresip 4.x / PortAudio migration
#

source dist/lib/versions.sh
source dist/lib/functions.sh

sl_prepare
sl_3rdparty

if [ "$BUILD_TARGET" != "windows64" ]; then
    echo "unsupported BUILD_TARGET: $BUILD_TARGET (only windows64)" >&2
    exit 1
fi

_arch="x86_64-w64-mingw32"
_proc="x86_64"

unset CC
unset CXX

if [ -z "$SL_SKIP_WEBUI" ]; then
    sl_get_webui
fi

if [ ! -d re-$re ]; then
    sl_get_libre
fi

if [ ! -d baresip-$baresip ]; then
    sl_get_baresip
fi

# Debian/Ubuntu ship win32 and posix thread variants of mingw-w64, use the
# posix one (like Arch/MSYS2, the 3rdparty prebuilds need libwinpthread)
_cc="$_arch-gcc"
_cxx="$_arch-g++"
if command -v $_arch-gcc-posix > /dev/null; then
    _cc="$_arch-gcc-posix"
    _cxx="$_arch-g++-posix"
fi

mingw_args="-DCMAKE_SYSTEM_NAME=Windows -DCMAKE_SYSTEM_PROCESSOR=$_proc"
mingw_args+=" -DCMAKE_C_COMPILER=$_cc -DCMAKE_CXX_COMPILER=$_cxx"
mingw_args+=" -DCMAKE_RC_COMPILER=$_arch-windres"
mingw_args+=" -DCMAKE_FIND_ROOT_PATH=/usr/$_arch"
mingw_args+=" -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER"
mingw_args+=" -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY"
mingw_args+=" -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY"

plugin_formats="CLAP;VST3"
if [ -n "$SL_SKIP_PLUGINS" ]; then
    plugin_formats=""
fi

sl_cmake_variant standalone $mingw_args
# DAW plugins, output in plugin/plugins and onair/plugins
sl_cmake_variant plugin $mingw_args "-DSL_PLUGIN_FORMATS=$plugin_formats"
sl_cmake_variant onair $mingw_args "-DSL_PLUGIN_FORMATS=$plugin_formats"

cp -a standalone/studio-link-standalone.exe .

mkdir -p debug
cp -a studio-link-standalone.exe debug
zip -r studio-link-debug.zip debug

$_arch-strip --strip-all studio-link-standalone.exe

cp -a studio-link-standalone.exe $artifacts/studio-link-standalone-$version_t.exe
cp -a studio-link-debug.zip $artifacts

# DAW plugins (one zip per variant with all formats)
if [ -n "$plugin_formats" ]; then
    for variant in plugin onair; do
        zip_name=studio-link-plugin
        [ "$variant" == "onair" ] && zip_name=studio-link-plugin-onair

        find $variant/plugins -type f \( -name "*.clap" -o -name "*.vst3" \) \
            -exec $_arch-strip --strip-unneeded {} \;

        rm -f $zip_name.zip
        (cd $variant/plugins && zip -qr ../../$zip_name.zip .)
        cp -a $zip_name.zip $artifacts
    done
fi
