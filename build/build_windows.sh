#!/bin/bash -ex

export PATH="$PATH:/usr/bin/core_perl"

source build/lib/versions.sh
source build/lib/functions.sh

sl_prepare

mkdir -p mingw
pushd mingw
mingwurl="https://github.com/Studio-Link-v2/mingw/releases/download/v18.01.0"
wget -N $mingwurl/cloog-0.18.5-1-x86_64.pkg.tar.xz
wget -N $mingwurl/isl-0.18-3-x86_64.pkg.tar.xz
wget -N $mingwurl/mingw-w64-binutils-2.29-1-x86_64.pkg.tar.xz
wget -N $mingwurl/mingw-w64-configure-0.1-1-any.pkg.tar.xz
wget -N $mingwurl/mingw-w64-crt-5.0.3-1-any.pkg.tar.xz
wget -N $mingwurl/mingw-w64-gcc-7.2.1.20180116-1-x86_64.pkg.tar.xz
wget -N $mingwurl/mingw-w64-headers-5.0.3-1-any.pkg.tar.xz
wget -N $mingwurl/mingw-w64-pkg-config-2-3-any.pkg.tar.xz
wget -N $mingwurl/mingw-w64-winpthreads-5.0.3-1-any.pkg.tar.xz
wget -N $mingwurl/osl-0.9.1-1-x86_64.pkg.tar.xz
yes | LANG=C sudo pacman -U *.pkg.tar.xz
popd

if [ "$BUILD_OS" == "windows32" ]; then
    _arch="i686-w64-mingw32"
else
    _arch="x86_64-w64-mingw32"
fi

unset CC
unset CXX


# Build RtAudio
#-----------------------------------------------------------------------------
if [ ! -d rtaudio-${rtaudio} ]; then
    sl_get_rtaudio
    pushd rtaudio-${rtaudio}
    export CPPFLAGS="-Wno-unused-function -Wno-unused-but-set-variable"
    ./autogen.sh --with-wasapi --host=${_arch}
    make
    unset CPPFLAGS
    cp -a .libs/librtaudio.a ../my_include/
    popd
fi

# Download openssl
#-----------------------------------------------------------------------------
if [ ! -d openssl-${openssl} ]; then
    sl_get_openssl
fi

# Build FLAC
#-----------------------------------------------------------------------------
if [ ! -d flac-${flac} ]; then
    sl_get_flac
    mkdir flac/build_win
    pushd flac/build_win
    ${_arch}-configure --disable-ogg --enable-static --disable-cpplibs
    make
    popd
    cp -a flac/include/FLAC my_include/
    cp -a flac/include/share my_include/
    cp -a flac/build_win/src/libFLAC/.libs/libFLAC.a my_include/
fi

# Build opus
#-----------------------------------------------------------------------------
if [ ! -d opus-$opus ]; then
    wget -N "https://archive.mozilla.org/pub/opus/opus-${opus}.tar.gz"
    tar -xzf opus-${opus}.tar.gz
    mkdir opus-$opus/build
    pushd opus-$opus/build
    ${_arch}-configure \
        --enable-custom-modes \
        --disable-doc \
        --disable-extra-programs
    make
    popd
    mkdir opus; cp opus-$opus/build/.libs/libopus.a opus/
    cp -a opus-$opus/include/*.h opus/
fi

# Download libre
#-----------------------------------------------------------------------------
if [ ! -d re-$re ]; then
    sl_get_libre
    mkdir -p my_include/re
    cp -a re/include/* my_include/re/
fi

# Download librem
#-----------------------------------------------------------------------------
if [ ! -d rem-$rem ]; then
    sl_get_librem
fi

# Download baresip with studio link addons
#-----------------------------------------------------------------------------
if [ ! -d baresip-$baresip ]; then
    sl_get_baresip
fi

# Download overlay-vst
#-----------------------------------------------------------------------------
if [ ! -d overlay-vst ]; then
    git clone https://github.com/Studio-Link/overlay-vst.git
    sed -i s/SLVMAJOR/$vmajor/ overlay-vst/version.h
    sed -i s/SLVMINOR/$vminor/ overlay-vst/version.h
    sed -i s/SLVPATCH/$vpatch/ overlay-vst/version.h
    wget http://www.steinberg.net/sdk_downloads/$vstsdk.zip
    unzip -q $vstsdk.zip
    mv VST_SDK/VST2_SDK overlay-vst/vstsdk2.4
fi

# Download overlay-onair-vst
#-----------------------------------------------------------------------------
if [ ! -d overlay-onair-vst ]; then
    git clone https://github.com/Studio-Link/overlay-onair-vst.git
    cp -a overlay-vst/vstsdk2.4 overlay-onair-vst/
fi

# Build
#-----------------------------------------------------------------------------

cp -a ../build/windows/Makefile .
make openssl
make
make -C overlay-vst PREFIX=$_arch
make -C overlay-onair-vst PREFIX=$_arch
zip -r studio-link-standalone-$BUILD_OS studio-link-standalone.exe
zip -r studio-link-plugin-$BUILD_OS overlay-vst/studio-link.dll
zip -r studio-link-plugin-onair-$BUILD_OS overlay-onair-vst/studio-link-onair.dll
ls -lha

s3_path="s3_upload/$TRAVIS_BRANCH/$version_t"
mkdir -p $s3_path
cp -a studio-link-standalone-$BUILD_OS.zip $s3_path
cp -a studio-link-plugin-$BUILD_OS.zip $s3_path
cp -a studio-link-plugin-onair-$BUILD_OS.zip $s3_path
