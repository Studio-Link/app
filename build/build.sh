#!/bin/bash

rem="0.4.6"
re="0.4.14"
opus="1.1"
openssl="1.0.2d"
baresip="master"
patch_url="https://github.com/Studio-Link-v2/baresip/compare/Studio-Link-v2:master"

echo "start build on $TRAVIS_OS_NAME"

mkdir -p src; cd src
mkdir -p my_include

if [ "$TRAVIS_OS_NAME" == "linux" ]; then
    my_extra_lflags="-L../openssl"
    my_extra_modules="alsa"
else
    my_openssl_osx="/usr/local/opt/openssl/lib/libcrypto.a "
    my_openssl_osx+="/usr/local/opt/openssl/lib/libssl.a"
    my_extra_lflags="-framework SystemConfiguration -framework CoreFoundation $my_openssl_osx"
    my_extra_modules="coreaudio"
fi


# Build openssl (linux only)
#-----------------------------------------------------------------------------
if [ "$TRAVIS_OS_NAME" == "linux" ]; then
    if [ ! -d openssl-${openssl} ]; then
        wget https://www.openssl.org/source/openssl-${openssl}.tar.gz
        tar -xzf openssl-${openssl}.tar.gz
        ln -s openssl-${openssl} openssl
        cd openssl
        ./config -fPIC shared
        make
        rm -f libcrypto.so
        rm -f libssl.so
        cd ..
        cp -a openssl/include/openssl ../my_include/
    fi
fi


# Build libre
#-----------------------------------------------------------------------------
if [ ! -d re-$re ]; then
    wget -N "http://www.creytiv.com/pub/re-${re}.tar.gz"
    tar -xzf re-${re}.tar.gz
    ln -s re-$re re
    if [ "$TRAVIS_OS_NAME" == "linux" ]; then
        cd re; make USE_OPENSSL=1 EXTRA_CFLAGS="-I ../my_include/" libre.a; cd ..
    else
        cd re
        make USE_OPENSSL=1 EXTRA_CFLAGS="-I /usr/local/opt/openssl/include" libre.a
        cd ..
    fi
    mkdir -p my_include/re
    cp -a re/include/* my_include/re/
fi


# Build librem
#-----------------------------------------------------------------------------
if [ ! -d rem-$rem ]; then
    wget -N "http://www.creytiv.com/pub/rem-${rem}.tar.gz"
    tar -xzf rem-${rem}.tar.gz
    ln -s rem-$rem rem
    cd rem; make librem.a; cd ..
fi


# Build opus
#-----------------------------------------------------------------------------
if [ ! -d opus-$opus ]; then
    wget -N "http://downloads.xiph.org/releases/opus/opus-${opus}.tar.gz"
    tar -xzf opus-${opus}.tar.gz
    cd opus-$opus; ./configure --with-pic; make; cd ..
    mkdir opus; cp opus-$opus/.libs/libopus.a opus/
    mkdir -p my_include/opus
    cp opus-$opus/include/*.h my_include/opus/ 
fi


# Build baresip with studio link addons
#-----------------------------------------------------------------------------
if [ ! -d baresip-$baresip ]; then
    git clone https://github.com/Studio-Link-v2/baresip.git baresip-$baresip
    ln -s baresip-$baresip baresip
    cp -a baresip-$baresip/include/baresip.h my_include/
    cd baresip-$baresip;

    ## Add patches
    curl ${patch_url}...studio-link-config.patch | patch -p1

    ## Link backend modules
    cp -a ../../webapp modules/webapp
    cp -a ../../effect modules/effect

    make LIBRE_SO=../re LIBREM_PATH=../rem STATIC=1 \
        MODULES="opus stdio ice g711 turn stun uuid webapp $my_extra_modules" \
        EXTRA_CFLAGS="-I ../my_include" EXTRA_LFLAGS="$my_extra_lflags -L ../opus"
    cp -a baresip ../studio-link-standalone
    make clean
    make LIBRE_SO=../re LIBREM_PATH=../rem STATIC=1 \
        MODULES="opus stdio ice g711 turn stun uuid webapp effect" \
        EXTRA_CFLAGS="-I ../my_include" EXTRA_LFLAGS="$my_extra_lflags -L ../opus" libbaresip.a
    cd ..
fi


# Build overlay-lv2 plugin (linux only)
#-----------------------------------------------------------------------------
if [ "$TRAVIS_OS_NAME" == "linux" ]; then
    if [ ! -d overlay-lv2 ]; then
        git clone https://github.com/Studio-Link-v2/overlay-lv2.git overlay-lv2
        cd overlay-lv2; ./build.sh; cd ..
        rm -Rf overlay-lv2/.git
        tar -cvzf overlay-lv2.tar.gz overlay-lv2
    fi
fi


# Build overlay-audio-unit plugin (osx only)
#-----------------------------------------------------------------------------
# @TODO



# Testing and prepare release upload
#-----------------------------------------------------------------------------

if [ "$TRAVIS_OS_NAME" == "linux" ]; then
    ldd studio-link-standalone
    mkdir -p lv2-plugin
    cp -a overlay-lv2/studio-link.so lv2-plugin/
    cp -a overlay-lv2/*.ttl lv2-plugin/
    cp -a overlay-lv2/README.md lv2-plugin/
    tar -czf studio-link-linux.tar.gz lv2-plugin studio-link-standalone
else
    otool -L studio-link-standalone
    tar -czf studio-link-osx.tar.gz studio-link-standalone
fi
./studio-link-standalone -t
