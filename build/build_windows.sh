#!/bin/bash -ex

version_t="v16.10.0-alpha0"
version_n="16.10.0"

#-----------------------------------------------------------------------------
rem="0.4.7"
re="master"
opus="1.1.3"
openssl="1.1.0c"
openssl_sha256="fc436441a2e05752d31b4e46115eb89709a28aef96d4fe786abe92409b2fd6f5"
baresip="master"
github_org="https://github.com/Studio-Link-v2"

mkdir src_windows
cd src_windows

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
    wget https://github.com/creytiv/re/archive/${re}.tar.gz
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

# Download baresip with studio link addons
#-----------------------------------------------------------------------------
if [ ! -d baresip-$baresip ]; then
    wget https://github.com/Studio-Link-v2/baresip/archive/$baresip.tar.gz
    tar -xzf $baresip.tar.gz
    ln -s baresip-$baresip baresip
    cp -a baresip-$baresip/include/baresip.h my_include/
    cd baresip-$baresip;

    ## Add patches
    patch -p1 < ../../build/patches/config.patch
    #patch -p1 < ../../build/patches/max_calls.patch
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

cp -a ../../build/windows/Makefile .
make openssl
make
ls -lha
