#!/bin/bash -ex

source build/versions.sh

if [ "$BUILD_OS" == "windows32" ] || [ "$BUILD_OS" == "windows64" ]; then
    curl -s https://raw.githubusercontent.com/mikkeloscar/arch-travis/master/arch-travis.sh | bash
    exit 0
fi

# Start build
#-----------------------------------------------------------------------------
echo "start build on $TRAVIS_OS_NAME"

mkdir -p src; cd src
mkdir -p my_include

sl_extra_lflags="-L ../opus -L ../my_include -L ../openssl "

if [ "$TRAVIS_OS_NAME" == "linux" ]; then
    openssl_target="linux-`uname -m`"
    sl_extra_modules="alsa jack"
    sed_opt="-i"
else
    openssl_target="darwin64-x86_64-cc"
    sl_extra_lflags+="-L ../openssl "
    sl_extra_lflags+="-framework SystemConfiguration "
    sl_extra_lflags+="-framework CoreFoundation"
    sl_extra_modules="audiounit"
    sed_opt="-i ''"
fi



# Build openssl
#-----------------------------------------------------------------------------
if [ ! -d openssl-${openssl} ]; then
    wget https://www.openssl.org/source/openssl-${openssl}.tar.gz
    echo "$openssl_sha256  openssl-${openssl}.tar.gz" | shasum -a 256 -c -
    tar -xzf openssl-${openssl}.tar.gz
    ln -s openssl-${openssl} openssl
    cd openssl
    ./Configure $openssl_target no-shared
    make build_libs
    cp -a include/openssl ../my_include/
    cd ..
fi


# Build FLAC
#-----------------------------------------------------------------------------
if [ ! -d flac-${flac} ]; then
    wget http://downloads.xiph.org/releases/flac/flac-${flac}.tar.xz
    tar -xf flac-${flac}.tar.xz 
    ln -s flac-${flac} flac
    cd flac
    ./configure --disable-ogg --enable-static
    make
    cp -a include/FLAC ../my_include/
    cp -a include/share ../my_include/
    cp -a src/libFLAC/.libs/libFLAC.a ../my_include/
    cd ..
fi


# Build opus
#-----------------------------------------------------------------------------
if [ ! -d opus-$opus ]; then
    wget -N "https://archive.mozilla.org/pub/opus/opus-${opus}.tar.gz"
    tar -xzf opus-${opus}.tar.gz
    cd opus-$opus; ./configure --with-pic; make; cd ..
    mkdir opus; cp opus-$opus/.libs/libopus.a opus/
    mkdir -p my_include/opus
    cp opus-$opus/include/*.h my_include/opus/ 
fi


# Build libre
#-----------------------------------------------------------------------------
if [ ! -d re-$re ]; then
    #wget -N "http://www.creytiv.com/pub/re-${re}.tar.gz"
    wget -N "https://github.com/creytiv/re/archive/v${re}.tar.gz"
    tar -xzf v${re}.tar.gz
    rm -f v${re}.tar.gz
    ln -s re-$re re
    cd re
    patch --ignore-whitespace -p1 < ../../build/patches/bluetooth_conflict.patch
    patch --ignore-whitespace -p1 < ../../build/patches/re_ice_bug.patch


    # WARNING build releases with RELEASE=1, because otherwise its MEM Debug
    # statements are not THREAD SAFE! on every platform, especilly windows.

    make $debug USE_OPENSSL=1 EXTRA_CFLAGS="-I ../my_include/" libre.a

    cd ..
    mkdir -p my_include/re
    cp -a re/include/* my_include/re/
fi


# Build librem
#-----------------------------------------------------------------------------
if [ ! -d rem-$rem ]; then
    #wget -N "http://www.creytiv.com/pub/rem-${rem}.tar.gz"
    wget -N "https://github.com/creytiv/rem/archive/v${rem}.tar.gz"
    tar -xzf v${rem}.tar.gz
    ln -s rem-$rem rem
    cd rem
    make $debug librem.a 
    cd ..
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
    patch -p1 < ../../build/patches/osx_sample_rate.patch
    
    #fixes multiple maxaverage lines in fmtp e.g.: 
    #fmtp: stereo=1;sprop-stereo=1;maxaveragebitrate=64000;maxaveragebitrate=64000;
    #after multiple module reloads it crashes because fmtp is to small(256 chars)
    #patch -p1 < ../../build/patches/opus_fmtp.patch

    ## Link backend modules
    cp -a ../../webapp modules/webapp
    cp -a ../../effect modules/effect
    cp -a ../../effectonair modules/effectonair
    cp -a ../../apponair modules/apponair
    
    sed $sed_opt s/SLVERSION_T/$version_t/ modules/webapp/webapp.c

    # Standalone
    make $debug LIBRE_SO=../re LIBREM_PATH=../rem STATIC=1 \
        MODULES="opus stdio ice g711 turn stun uuid auloop webapp $sl_extra_modules" \
        EXTRA_CFLAGS="-I ../my_include" \
        EXTRA_LFLAGS="$sl_extra_lflags"

    cp -a baresip ../studio-link-standalone

    # libbaresip.a without effect plugin
    make $debug LIBRE_SO=../re LIBREM_PATH=../rem STATIC=1 \
        MODULES="opus stdio ice g711 turn stun uuid auloop webapp $sl_extra_modules" \
        EXTRA_CFLAGS="-I ../my_include" \
        EXTRA_LFLAGS="$sl_extra_lflags" libbaresip.a
    cp -a libbaresip.a ../my_include/libbaresip_standalone.a

    # Effectonair Plugin
    make clean
    make $debug LIBRE_SO=../re LIBREM_PATH=../rem STATIC=1 \
        MODULES="opus stdio ice g711 turn stun uuid auloop apponair effectonair" \
        EXTRA_CFLAGS="-I ../my_include -DSLIVE" \
        EXTRA_LFLAGS="$sl_extra_lflags" libbaresip.a
    cp -a libbaresip.a ../my_include/libbaresip_onair.a

    # Effect Plugin
    make clean
    make $debug LIBRE_SO=../re LIBREM_PATH=../rem STATIC=1 \
        MODULES="opus stdio ice g711 turn stun uuid auloop webapp effect" \
        EXTRA_CFLAGS="-I ../my_include -DSLPLUGIN" \
        EXTRA_LFLAGS="$sl_extra_lflags" libbaresip.a

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


# Build overlay-onair-lv2 plugin (linux only)
#-----------------------------------------------------------------------------
if [ "$TRAVIS_OS_NAME" == "linux" ]; then
    if [ ! -d overlay-onair-lv2 ]; then
        git clone $github_org/overlay-onair-lv2.git overlay-onair-lv2
        cd overlay-onair-lv2; ./build.sh; cd ..
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


# Build overlay-audio-unit plugin (osx only)
#-----------------------------------------------------------------------------
if [ "$TRAVIS_OS_NAME" == "osx" ]; then
    if [ ! -d overlay-onair-au ]; then
        git clone \
            $github_org/overlay-onair-au.git overlay-onair-au
        cd overlay-onair-au
        sed -i '' s/SLVERSION_N/$version_n/ StudioLinkOnAir/StudioLinkOnAir.jucer
        cp -a ../overlay-audio-unit/JUCE .
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
    mkdir -p studio-link.lv2
    cp -a overlay-lv2/studio-link.so studio-link.lv2/
    cp -a overlay-lv2/*.ttl studio-link.lv2/
    cp -a overlay-lv2/README.md studio-link.lv2/
    zip -r studio-link-plugin-linux studio-link.lv2

    mkdir -p studio-link-onair.lv2
    cp -a overlay-onair-lv2/studio-link-onair.so studio-link-onair.lv2/
    cp -a overlay-onair-lv2/*.ttl studio-link-onair.lv2/
    cp -a overlay-onair-lv2/README.md studio-link-onair.lv2/
    zip -r studio-link-plugin-onair-linux studio-link-onair.lv2

    zip -r studio-link-standalone-linux studio-link-standalone
else
    otool -L studio-link-standalone
    cp -a ~/Library/Audio/Plug-Ins/Components/StudioLink.component StudioLink.component
    cp -a ~/Library/Audio/Plug-Ins/Components/StudioLinkOnAir.component StudioLinkOnAir.component
    mv overlay-standalone-osx/build/Release/StudioLinkStandalone.app StudioLinkStandalone.app
    codesign -f --verbose -s "Developer ID Application: Sebastian Reimers (CX34XZ2JTT)" --keychain ~/Library/Keychains/sl-build.keychain StudioLinkStandalone.app
    sed $sed_opt s/ITSR:\ StudioLinkOnAir/StudioLinkOnAir\ \(ITSR\)/ StudioLinkOnAir.component/Contents/Info.plist # Reaper 5.70 Fix
    zip -r studio-link-plugin-osx StudioLink.component
    zip -r studio-link-plugin-onair-osx StudioLinkOnAir.component
    zip -r studio-link-standalone-osx StudioLinkStandalone.app
    #security delete-keychain ~/Library/Keychains/sl-build.keychain
fi
