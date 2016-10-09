#!/bin/bash -ex

version_t="v16.10.0-alpha0"
version_n="16.10.0"

#-----------------------------------------------------------------------------
rem="0.4.7"
re="0.4.15"
opus="1.1.2"
openssl="1.0.2j"
baresip="master"
juce="4.1.0"
github_org="https://github.com/Studio-Link-v2"

# Start build
#-----------------------------------------------------------------------------
echo "start build on $TRAVIS_OS_NAME"

mkdir -p src; cd src
mkdir -p my_include

if [ "$TRAVIS_OS_NAME" == "linux" ]; then

    sl_extra_lflags="-L../openssl"
    sl_extra_modules="alsa jack"

else
    sl_openssl_osx="/usr/local/opt/openssl/lib/libcrypto.a "
    sl_openssl_osx+="/usr/local/opt/openssl/lib/libssl.a"
    
    sl_extra_lflags="-framework SystemConfiguration "
    sl_extra_lflags+="-framework CoreFoundation $sl_openssl_osx"
    sl_extra_modules="audiounit"
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
        cp -a include/openssl ../my_include/
        cd ..
    fi
fi


# Build libre
#-----------------------------------------------------------------------------
if [ ! -d re-$re ]; then
    wget -N "http://www.creytiv.com/pub/re-${re}.tar.gz"
    tar -xzf re-${re}.tar.gz
    ln -s re-$re re
    cd re
    patch --ignore-whitespace -p1 < ../../build/patches/bluetooth_conflict.patch
    patch --ignore-whitespace -p1 < ../../build/patches/re_ice_bug.patch

    if [ "$TRAVIS_OS_NAME" == "linux" ]; then
        make USE_OPENSSL=1 EXTRA_CFLAGS="-I ../my_include/" libre.a
    else
        make USE_OPENSSL=1 \
            EXTRA_CFLAGS="-I /usr/local/opt/openssl/include" libre.a
    fi

    cd ..
    mkdir -p my_include/re
    cp -a re/include/* my_include/re/
fi


# Build librem
#-----------------------------------------------------------------------------
if [ ! -d rem-$rem ]; then
    wget -N "http://www.creytiv.com/pub/rem-${rem}.tar.gz"
    tar -xzf rem-${rem}.tar.gz
    ln -s rem-$rem rem
    cd rem
    make librem.a 
    cd ..
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
    wget https://github.com/Studio-Link-v2/baresip/archive/$baresip.tar.gz
    tar -xzf $baresip.tar.gz
    ln -s baresip-$baresip baresip
    cp -a baresip-$baresip/include/baresip.h my_include/
    cd baresip-$baresip;

    ## Add patches
    patch -p1 < ../../build/patches/config.patch
    patch -p1 < ../../build/patches/max_calls.patch
    patch -p1 < ../../build/patches/osx_sample_rate.patch

    ## Link backend modules
    cp -a ../../webapp modules/webapp
    cp -a ../../effect modules/effect

    # Standalone
    make LIBRE_SO=../re LIBREM_PATH=../rem STATIC=1 \
        MODULES="opus stdio ice g711 turn stun uuid auloop webapp $sl_extra_modules" \
        EXTRA_CFLAGS="-I ../my_include" \
        EXTRA_LFLAGS="$sl_extra_lflags -L ../opus"

    cp -a baresip ../studio-link-standalone

    # libbaresip.a without effect plugin
    make LIBRE_SO=../re LIBREM_PATH=../rem STATIC=1 \
        MODULES="opus stdio ice g711 turn stun uuid auloop webapp $sl_extra_modules" \
        EXTRA_CFLAGS="-I ../my_include" \
        EXTRA_LFLAGS="$sl_extra_lflags -L ../opus" libbaresip.a
    cp -a libbaresip.a ../my_include/libbaresip_standalone.a

    # Effect Plugin
    make clean
    make LIBRE_SO=../re LIBREM_PATH=../rem STATIC=1 \
        MODULES="opus stdio ice g711 turn stun uuid auloop webapp effect" \
        EXTRA_CFLAGS="-I ../my_include -DSLPLUGIN" \
        EXTRA_LFLAGS="$sl_extra_lflags -L ../opus" libbaresip.a
    cd ..
fi


# Build overlay-lv2 plugin (linux only)
#-----------------------------------------------------------------------------
if [ "$TRAVIS_OS_NAME" == "linux" ]; then
    if [ ! -d overlay-lv2 ]; then
        git clone $github_org/overlay-lv2.git overlay-lv2
        cd overlay-lv2; ./build.sh; cd ..
    fi
fi


# Build overlay-audio-unit plugin (osx only)
#-----------------------------------------------------------------------------
if [ "$TRAVIS_OS_NAME" == "osx" ]; then
    if [ ! -d overlay-audio-unit ]; then
        git clone \
            $github_org/overlay-audio-unit.git overlay-audio-unit
        cd overlay-audio-unit
        sed -i '' s/SLVERSION_N/$version_n/ StudioLink/StudioLink.jucer
        wget https://github.com/julianstorer/JUCE/archive/$juce.tar.gz
        tar -xzf $juce.tar.gz
        rm -Rf JUCE
        mv JUCE-$juce JUCE
        ./build.sh
        cd ..
    fi
fi


# Build standalone app bundle (osx only)
#-----------------------------------------------------------------------------
if [ "$TRAVIS_OS_NAME" == "osx" ]; then
    if [ ! -d overlay-standalone-osx ]; then
        git clone \
            $github_org/overlay-standalone-osx.git overlay-standalone-osx
        cp -a my_include/re overlay-standalone-osx/StudioLinkStandalone/
        cp -a my_include/baresip.h \
            overlay-standalone-osx/StudioLinkStandalone/
        cd overlay-standalone-osx
        sed -i '' s/SLVERSION_N/$version_n/ StudioLinkStandalone/Info.plist
        ./build.sh
        cd ..
    fi
fi


# Testing and prepare release upload
#-----------------------------------------------------------------------------

./studio-link-standalone -t

if [ "$TRAVIS_OS_NAME" == "linux" ]; then
    ldd studio-link-standalone
    mkdir -p lv2-plugin
    cp -a overlay-lv2/studio-link.so lv2-plugin/
    cp -a overlay-lv2/*.ttl lv2-plugin/
    cp -a overlay-lv2/README.md lv2-plugin/
    zip -r studio-link-plugin-linux lv2-plugin
    zip -r studio-link-standalone-linux studio-link-standalone
else
    otool -L studio-link-standalone
    cp -a ~/Library/Audio/Plug-Ins/Components/StudioLink.component StudioLink.component
    mv overlay-standalone-osx/build/Release/StudioLinkStandalone.app StudioLinkStandalone.app
    codesign -f --verbose -s "Developer ID Application: Sebastian Reimers (CX34XZ2JTT)" --keychain ~/Library/Keychains/sl-build.keychain StudioLinkStandalone.app
    zip -r studio-link-plugin-osx StudioLink.component
    zip -r studio-link-standalone-osx StudioLinkStandalone.app
    #security delete-keychain ~/Library/Keychains/sl-build.keychain
fi
