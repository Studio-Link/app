#!/bin/bash

rem="0.4.6"
re="0.4.14"
opus="1.1"
baresip="master"
patch_url="https://github.com/Studio-Link-v2/baresip/compare/Studio-Link-v2:master"

if [ "$TRAVIS_OS_NAME" == "linux" ]; then
    my_extra_lflags=""
else
    my_extra_lflags="-framework SystemConfiguration -framework CoreFoundation"
fi

# Build libre ------
wget -N "http://www.creytiv.com/pub/re-0.4.14.tar.gz"
tar -xzf re-${re}.tar.gz
ln -s re-$re re
cd re; make libre.a; cd ..

# Build librem ------
wget -N "http://www.creytiv.com/pub/rem-0.4.6.tar.gz"
tar -xzf rem-${rem}.tar.gz
ln -s rem-$rem rem
cd rem; make librem.a; cd ..

# Build opus ------
wget -N "http://downloads.xiph.org/releases/opus/opus-1.1.tar.gz"
tar -xzf opus-${opus}.tar.gz
cd opus-$opus; ./configure; make; cd ..
mkdir opus; cp opus-$opus/.libs/libopus.a opus/
mkdir -p my_include/opus
cp opus-$opus/include/*.h my_include/opus/ 

# Build baresip with studio link addons
git clone https://github.com/Studio-Link-v2/baresip.git baresip-master
cd baresip-$baresip;

## Add patches
curl ${patch_url}...studio-link-config.patch | patch -p1

## Link backend modules
ln -s ../webapp modules/webapp

make LIBRE_SO=../re LIBREM_PATH=../rem STATIC=1 \
    MODULES="opus stdio ice menu g711 turn stun uuid auloop contact webapp" \
    EXTRA_CFLAGS="-I ../my_include" EXTRA_LFLAGS="$my_extra_lflags -L ../opus"
cd ..

if [ "$TRAVIS_OS_NAME" == "linux" ]; then
    ldd baresip-$baresip/baresip
    cp baresip-$baresip/baresip baresip-linux
else
    otool -L baresip-$baresip/baresip
    cp baresip-$baresip/baresip baresip-osx
fi
baresip-$baresip/baresip -t
ls -lha ~/.studio-link
