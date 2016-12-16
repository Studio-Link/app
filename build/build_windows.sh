#!/bin/bash -ex

version_t="v16.12.0-beta"
version_n="16.12.0"

#-----------------------------------------------------------------------------
rem="0.4.7"
re="0.5.0"
opus="1.1.3"
openssl="1.1.0c"
openssl_sha256="fc436441a2e05752d31b4e46115eb89709a28aef96d4fe786abe92409b2fd6f5"
baresip="master"
github_org="https://github.com/Studio-Link-v2"
flac="1.3.1"

mkdir -p src
cd src
mkdir -p my_include

if [ "$BUILD_OS" == "windows32" ]; then
    _arch="i686-w64-mingw32"
else
    _arch="x86_64-w64-mingw32"
fi

# Download openssl
#-----------------------------------------------------------------------------
if [ ! -d openssl-${openssl} ]; then
    wget https://www.openssl.org/source/openssl-${openssl}.tar.gz
    echo "$openssl_sha256  openssl-${openssl}.tar.gz" | \
        /usr/bin/core_perl/shasum -a 256 -c -
    tar -xzf openssl-${openssl}.tar.gz
    ln -s openssl-${openssl} openssl
fi

# Build FLAC
#-----------------------------------------------------------------------------
if [ ! -d flac-${flac} ]; then
    wget http://downloads.xiph.org/releases/flac/flac-${flac}.tar.xz
    tar -xf flac-${flac}.tar.xz 
    ln -s flac-${flac} flac
    mkdir flac/build_win
    pushd flac/build_win
    ${_arch}-configure --disable-ogg --enable-static
    make
    popd
    cp -a flac/include/FLAC my_include/
    cp -a flac/include/share my_include/
    cp -a flac/build_win/src/libFLAC/.libs/libFLAC.a my_include/
fi

# Download libre
#-----------------------------------------------------------------------------
if [ ! -d re-$re ]; then
    wget -N "http://www.creytiv.com/pub/re-${re}.tar.gz"
    #wget https://github.com/creytiv/re/archive/${re}.tar.gz
    tar -xzf re-${re}.tar.gz
    rm -f re-${re}.tar.gz
    ln -s re-$re re
    pushd re
    patch --ignore-whitespace -p1 < ../../build/patches/bluetooth_conflict.patch
    patch --ignore-whitespace -p1 < ../../build/patches/re_ice_bug.patch
    patch -p1 < ../../build/patches/fix_windows_ssize_t_bug.patch
    popd
    mkdir -p my_include/re
    cp -a re/include/* my_include/re/
fi

# Download librem
#-----------------------------------------------------------------------------
if [ ! -d rem-$rem ]; then
    wget -N "http://www.creytiv.com/pub/rem-${rem}.tar.gz"
    tar -xzf rem-${rem}.tar.gz
    ln -s rem-$rem rem
fi

# Build opus
#-----------------------------------------------------------------------------
if [ ! -d opus-$opus ]; then
    wget -N "http://downloads.xiph.org/releases/opus/opus-${opus}.tar.gz"
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


# Download baresip with studio link addons
#-----------------------------------------------------------------------------
if [ ! -d baresip-$baresip ]; then
    wget https://github.com/Studio-Link-v2/baresip/archive/$baresip.tar.gz
    tar -xzf $baresip.tar.gz
    ln -s baresip-$baresip baresip
    cp -a baresip-$baresip/include/baresip.h my_include/
    pushd baresip-$baresip

    ## Add patches
    patch -p1 < ../../build/patches/config.patch
    patch -p1 < ../../build/patches/osx_sample_rate.patch

    ## Link backend modules
    cp -a ../../webapp modules/webapp
    cp -a ../../effect modules/effect
    cp -a ../../effectlive modules/effectlive
    cp -a ../../applive modules/applive
    popd
fi

# Download overlay-vst
#-----------------------------------------------------------------------------
if [ ! -d overlay-vst ]; then
    git clone https://github.com/Studio-Link-v2/overlay-vst.git
    wget http://www.steinberg.net/sdk_downloads/vstsdk366_27_06_2016_build_61.zip
    unzip -q vstsdk366_27_06_2016_build_61.zip
    mv VST3\ SDK overlay-vst/vstsdk2.4
fi

# Build
#-----------------------------------------------------------------------------

cp -a ../build/windows/Makefile .
make openssl
make
sudo pacman --noconfirm -S mingw-w64-winpthreads
make -C overlay-vst PREFIX=$_arch
zip -r studio-link-standalone-$BUILD_OS studio-link-standalone.exe
zip -r studio-link-plugin-$BUILD_OS overlay-vst/studio-link.dll
ls -lha
