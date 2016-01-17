#!/bin/bash

PREFIX="x86_64-w64-mingw32"
#PREFIX="i686-w64-mingw32"
baresip="master"
github_org="https://github.com/Studio-Link-v2"
patch_url="$github_org/baresip/compare/Studio-Link-v2:master"

cd re
make CC=/usr/bin/$PREFIX-gcc OS=win32 clean
make CC=/usr/bin/$PREFIX-gcc OS=win32 EXTRA_CFLAGS="-DFD_SETSIZE=2000" libre.a
cd ..

cd rem
make CC=/usr/bin/$PREFIX-gcc OS=win32 clean
make CC=/usr/bin/$PREFIX-gcc OS=win32 librem.a
cd ..

if [ ! -d baresip-$baresip ]; then
    git clone $github_org/baresip.git baresip-$baresip
    ln -s baresip-$baresip baresip
    cd baresip-$baresip;


    ## Add patches
    curl ${patch_url}...studio-link-config.patch | patch -p1

    ## Link backend modules
    cp -a ../../backend/webapp modules/webapp
    cp -a ../../backend/effect modules/effect

fi

cd baresip
make CC=/usr/bin/$PREFIX-gcc OS=win32 clean
# Standalone
make CC=/usr/bin/$PREFIX-gcc OS=win32 LIBRE_SO=../re LIBREM_PATH=../rem STATIC=1 \
    MODULES="opus wincons ice g711 turn stun uuid auloop webapp winwave" \
    EXTRA_LFLAGS="-static"

cp -a baresip.exe ../studio-link-$PREFIX.exe

make CC=/usr/bin/$PREFIX-gcc OS=win32 clean
rm libbaresip.a
make CC=/usr/bin/$PREFIX-gcc OS=win32 LIBRE_SO=../re LIBREM_PATH=../rem STATIC=1 EXTRA_CFLAGS="-DSLPLUGIN" \
    MODULES="opus ice g711 turn stun uuid webapp effect" libbaresip.a 
cd ..

cd overlay-vst
make PREFIX=$PREFIX
cd ..

zip studio-link-$PREFIX.zip studio-link-$PREFIX.exe overlay-vst/studio-link.dll 
