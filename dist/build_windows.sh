#!/bin/bash -ex
#
# Cross build for Windows with mingw-w64 (BUILD_TARGET: windows32|windows64)
#
# NOTE: not tested yet after the baresip 4.x / PortAudio migration
#

export PATH="$PATH:/usr/bin/core_perl"

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

mingw_args="-DCMAKE_SYSTEM_NAME=Windows -DCMAKE_SYSTEM_PROCESSOR=$_proc"
mingw_args+=" -DCMAKE_C_COMPILER=$_arch-gcc -DCMAKE_CXX_COMPILER=$_arch-g++"
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

s3_path="s3_upload/$GIT_BRANCH/$version_t/$BUILD_TARGET"
mkdir -p $s3_path
cp -a studio-link-standalone.exe $s3_path/studio-link-standalone-$version_t.exe
cp -a studio-link-debug.zip $s3_path
