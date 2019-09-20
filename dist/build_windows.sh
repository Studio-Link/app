#!/bin/bash -ex

export PATH="$PATH:/usr/bin/core_perl"

source dist/lib/versions.sh
source dist/lib/functions.sh

sl_prepare

make_opts="-j4"

mkdir -p mingw
pushd mingw
mingwurl="https://github.com/Studio-Link/mingw/releases/download/v18.01.0"
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


cp -a ../dist/windows/Makefile .
make
make -C overlay-vst PREFIX=$_arch
make -C overlay-onair-vst PREFIX=$_arch

$_arch-strip --strip-all studio-link-standalone.exe
$_arch-strip --strip-all overlay-vst/studio-link.dll
$_arch-strip --strip-all overlay-onair-vst/studio-link-onair.dll

zip -r studio-link-plugin-$BUILD_OS overlay-vst/studio-link.dll
zip -r studio-link-plugin-onair-$BUILD_OS overlay-onair-vst/studio-link-onair.dll
ls -lha

s3_path="s3_upload/$TRAVIS_BRANCH/$version_t/$BUILD_OS"
mkdir -p $s3_path
cp -a studio-link-standalone.exe $s3_path/studio-link-standalone-$version_tc.exe
cp -a studio-link-plugin-$BUILD_OS.zip $s3_path
cp -a studio-link-plugin-onair-$BUILD_OS.zip $s3_path
