#!/bin/bash

rem="0.4.6"
re="0.4.14"
opus="1.1"
baresip="master"
patch_url="https://github.com/Studio-Link-v2/baresip/compare/Studio-Link-v2:master"

echo "start build on $TRAVIS_OS_NAME"

if [ "$TRAVIS_OS_NAME" == "linux" ]; then
    my_extra_lflags=""
else
    my_extra_lflags="-framework SystemConfiguration -framework CoreFoundation"
fi


# Build libre
#-----------------------------------------------------------------------------
wget -N "http://www.creytiv.com/pub/re-${re}.tar.gz"
tar -xzf re-${re}.tar.gz
ln -s re-$re re
cd re; make libre.a; cd ..


# Build librem
#-----------------------------------------------------------------------------
wget -N "http://www.creytiv.com/pub/rem-${rem}.tar.gz"
tar -xzf rem-${rem}.tar.gz
ln -s rem-$rem rem
cd rem; make librem.a; cd ..


# Build opus
#-----------------------------------------------------------------------------
wget -N "http://downloads.xiph.org/releases/opus/opus-${opus}.tar.gz"
tar -xzf opus-${opus}.tar.gz
cd opus-$opus; ./configure; make; cd ..
mkdir opus; cp opus-$opus/.libs/libopus.a opus/
mkdir -p my_include/opus
cp opus-$opus/include/*.h my_include/opus/ 


# Build baresip with studio link addons
#-----------------------------------------------------------------------------
git clone https://github.com/Studio-Link-v2/baresip.git baresip-master
ln -s baresip-$baresip baresip
cd baresip-$baresip;

## Add patches
curl ${patch_url}...studio-link-config.patch | patch -p1

## Link backend modules
cp -a ../webapp modules/webapp
cp -a ../effect modules/effect

make LIBRE_SO=../re LIBREM_PATH=../rem STATIC=1 \
    MODULES="opus stdio ice g711 turn stun uuid webapp" \
    EXTRA_CFLAGS="-I ../my_include" EXTRA_LFLAGS="$my_extra_lflags -L ../opus"
make LIBRE_SO=../re LIBREM_PATH=../rem STATIC=1 \
    MODULES="opus stdio ice g711 turn stun uuid webapp effect" \
    EXTRA_CFLAGS="-I ../my_include" EXTRA_LFLAGS="$my_extra_lflags -L ../opus" libbaresip.a
cd ..


# Build overlay-lv2 plugin (linux only)
#-----------------------------------------------------------------------------
if [ "$TRAVIS_OS_NAME" == "linux" ]; then
    git clone https://github.com/Studio-Link-v2/overlay-lv2.git overlay-lv2
    cp -a baresip/include/baresip.h overlay-lv2
    mkdir overlay-lv2/re
    cp -a re/include/* overlay-lv2/re/
    cd overlay-lv2
    ./build.sh
    cd ..
    tar -cvzf overlay-lv2.tar.gz overlay-lv2
fi


# Build overlay-audio-unit plugin (osx only)
#-----------------------------------------------------------------------------
# @TODO



# Testing and prepare release upload
#-----------------------------------------------------------------------------

if [ "$TRAVIS_OS_NAME" == "linux" ]; then
    ldd baresip-$baresip/baresip
    cp baresip-$baresip/baresip studio-link-linux
else
    otool -L baresip-$baresip/baresip
    cp baresip-$baresip/baresip studio-link-osx
fi
baresip-$baresip/baresip -t
