#!/bin/bash -ex

version_t="v16.10.0-alpha0"
version_n="16.10.0"

#-----------------------------------------------------------------------------
rem="0.4.7"
re="0.5.0"
opus="1.1.3"
openssl="1.1.0c"
openssl_sha256="fc436441a2e05752d31b4e46115eb89709a28aef96d4fe786abe92409b2fd6f5"
baresip="master"
github_org="https://github.com/Studio-Link-v2"

mkdir -p src
cd src

# Download openssl
#-----------------------------------------------------------------------------
if [ ! -d openssl-${openssl} ]; then
    wget https://www.openssl.org/source/openssl-${openssl}.tar.gz
    echo "$openssl_sha256  openssl-${openssl}.tar.gz" | \
        /usr/bin/core_perl/shasum -a 256 -c -
    tar -xzf openssl-${openssl}.tar.gz
    ln -s openssl-${openssl} openssl
fi

# Download libre
#-----------------------------------------------------------------------------
if [ ! -d re-$re ]; then
    wget -N "http://www.creytiv.com/pub/re-${re}.tar.gz"
    #wget https://github.com/creytiv/re/archive/${re}.tar.gz
    tar -xzf ${re}.tar.gz
    rm -f ${re}.tar.gz
    ln -s re-$re re
    cd re
    patch --ignore-whitespace -p1 < ../../build/patches/bluetooth_conflict.patch
    patch --ignore-whitespace -p1 < ../../build/patches/re_ice_bug.patch
    cd ..
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
    if [ "$BUILD_OS" == "windows32" ]; then
        _arch="i686-w64-mingw32"
    else
        _arch="x86_64-w64-mingw32"
    fi
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
    cd baresip-$baresip;

    ## Add patches
    patch -p1 < ../../build/patches/config.patch
    patch -p1 < ../../build/patches/osx_sample_rate.patch

    ## Link backend modules
    cp -a ../../webapp modules/webapp
    cp -a ../../effect modules/effect
    cp -a ../../effectlive modules/effectlive
    cp -a ../../applive modules/applive
    cd ..
fi

# Build
#-----------------------------------------------------------------------------

cp -a ../build/windows/Makefile .
make openssl
make
zip -r studio-link-standalone-$BUILD_OS studio-link-standalone.exe
ls -lha
