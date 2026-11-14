#!/bin/bash -ex
#
# Cross build for Windows with mingw-w64 (BUILD_TARGET: windows32|windows64)
#
# NOTE: not tested yet after the baresip 4.x / PortAudio migration
#

source dist/lib/versions.sh
source dist/lib/functions.sh

sl_prepare
sl_3rdparty

if [ "$BUILD_TARGET" == "windows32" ]; then
    _arch="i686-w64-mingw32"
    _proc="i686"
else
    _arch="x86_64-w64-mingw32"
    _proc="x86_64"
fi

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

sl_cmake_variant standalone $mingw_args
sl_cmake_variant plugin $mingw_args
sl_cmake_variant onair $mingw_args

cp -a standalone/studio-link-standalone.exe .

mkdir -p debug
cp -a studio-link-standalone.exe debug
zip -r studio-link-debug.zip debug

$_arch-strip --strip-all studio-link-standalone.exe

# DAW plugins (src/plugin, CLAP/VST3 via clap-wrapper) are not built for
# Windows yet, the MinGW cross build of clap-wrapper/VST3 SDK is untested

cp -a studio-link-standalone.exe $artifacts/studio-link-standalone-$version_t.exe
cp -a studio-link-debug.zip $artifacts
