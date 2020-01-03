#!/bin/bash -ex

source dist/lib/versions.sh
source dist/lib/functions.sh

make_opts="-j4"

if [ "$BUILD_OS" == "ccheck" ]; then
    #dist/tools/ccheck.py src/modules/webapp
    dist/tools/ccheck.py src/modules/slrtaudio/record.c
    dist/tools/ccheck.py src/modules/slrtaudio/
    exit 0
fi

if [ "$BUILD_OS" == "linuxarm" ]; then
    git clone https://github.com/raspberrypi/tools $HOME/rpi-tools
    export PATH=$PATH:$HOME/rpi-tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64/bin
    export ARCH=arm
    export CCPREFIX=$HOME/rpi-tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64/bin/arm-linux-gnueabihf-
    export CC=arm-linux-gnueabihf-gcc
    export RPI_CROSS_COMPILE=true
fi

if [ "$BUILD_OS" == "windows32" ] || [ "$BUILD_OS" == "windows64" ]; then
    curl -s https://raw.githubusercontent.com/studio-link-3rdparty/arch-travis/master/arch-travis.sh | bash
    #curl -s https://raw.githubusercontent.com/mikkeloscar/arch-travis/master/arch-travis.sh | bash
    exit 0
fi

# Start build
#-----------------------------------------------------------------------------
sl_prepare
sl_3rdparty

sl_extra_lflags="-L ../opus -L ../my_include "

if [ "$BUILD_OS" == "linux" ]; then
    sl_extra_modules="alsa slrtaudio"
fi
if [ "$BUILD_OS" == "linuxjack" ]; then
    sl_extra_modules="jack alsa slrtaudio"
fi
if [ "$TRAVIS_OS_NAME" == "osx" ]; then 
    export MACOSX_DEPLOYMENT_TARGET=10.9
    sl_extra_lflags+="-L ../openssl ../openssl/libssl.a ../openssl/libcrypto.a "
    sl_extra_lflags+="-framework SystemConfiguration "
    sl_extra_lflags+="-framework CoreFoundation"
    sl_extra_modules="slrtaudio"
    sed_opt="-i ''"
fi

sl_extra_modules="$sl_extra_modules g722"

# Build libre
#-----------------------------------------------------------------------------
if [ ! -d re-$re ]; then
    sl_get_libre

    # WARNING build releases with RELEASE=1, because otherwise its MEM Debug
    # statements are not THREAD SAFE! on every platform, especilly windows.
    make -C re $make_opts $debug USE_OPENSSL="yes" EXTRA_CFLAGS="-I ../my_include/" libre.a
    mkdir -p my_include/re
    cp -a re/include/* my_include/re/
fi

# Build librem
#-----------------------------------------------------------------------------
if [ ! -d rem-$rem ]; then
    sl_get_librem
    cd rem
    make $debug librem.a 
    cd ..
fi

# Build baresip with studio link addons
#-----------------------------------------------------------------------------
if [ ! -d baresip-$baresip ]; then
    sl_get_baresip

    pushd baresip-$baresip
    # Standalone
    if [ "$BUILD_OS" == "linux" ] || [ "$BUILD_OS" == "linuxjack" ]; then
        make $debug $make_opts USE_OPENSSL="yes" LIBRE_SO=../re LIBREM_PATH=../rem STATIC=1 \
            MODULES="opus stdio ice g711 turn stun uuid auloop webapp $sl_extra_modules" \
            EXTRA_CFLAGS="-I ../my_include" \
            EXTRA_LFLAGS="$sl_extra_lflags -L ../openssl"

        cp -a baresip ../studio-link-standalone
    fi

    # libbaresip.a without effect plugin
    make $debug $make_opts USE_OPENSSL="yes" LIBRE_SO=../re LIBREM_PATH=../rem STATIC=1 \
        MODULES="opus stdio ice g711 turn stun uuid auloop webapp $sl_extra_modules" \
        EXTRA_CFLAGS="-I ../my_include" \
        EXTRA_LFLAGS="$sl_extra_lflags" libbaresip.a
    cp -a libbaresip.a ../my_include/libbaresip_standalone.a

    # Effectonair Plugin
    make clean
    make $debug $make_opts USE_OPENSSL="yes" LIBRE_SO=../re LIBREM_PATH=../rem STATIC=1 \
        MODULES="opus stdio ice g711 turn stun uuid auloop apponair effectonair" \
        EXTRA_CFLAGS="-I ../my_include -DSLIVE" \
        EXTRA_LFLAGS="$sl_extra_lflags" libbaresip.a
    cp -a libbaresip.a ../my_include/libbaresip_onair.a

    # Effect Plugin
    make clean
    make $debug $make_opts USE_OPENSSL="yes" LIBRE_SO=../re LIBREM_PATH=../rem STATIC=1 \
        MODULES="opus stdio ice g711 turn stun uuid auloop webapp effect" \
        EXTRA_CFLAGS="-I ../my_include -DSLPLUGIN" \
        EXTRA_LFLAGS="$sl_extra_lflags" libbaresip.a

    popd
fi

# Build overlay-lv2 plugin (linux only)
#-----------------------------------------------------------------------------
if [ "$BUILD_OS" == "linux" ]; then
    if [ ! -d overlay-lv2 ]; then
        git clone $github_org/overlay-lv2.git overlay-lv2
        cd overlay-lv2; ./build.sh; cd ..
    fi
fi

# Build overlay-onair-lv2 plugin (linux only)
#-----------------------------------------------------------------------------
if [ "$BUILD_OS" == "linux" ]; then
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
        #wget https://github.com/julianstorer/JUCE/archive/$juce.tar.gz
        wget https://d30pueezughrda.cloudfront.net/juce/juce-$juce-osx.zip
        unzip juce-$juce-osx.zip
        ./build.sh
        cd ..
    fi
fi

# Build overlay-onair-au plugin (osx only)
#-----------------------------------------------------------------------------
if [ "$TRAVIS_OS_NAME" == "osx" ]; then
    if [ ! -d overlay-onair-au ]; then
        git clone \
            $github_org/overlay-onair-au.git overlay-onair-au
        cd overlay-onair-au
        sed -i '' s/SLVERSION_N/$version_n/ StudioLinkOnAir/StudioLinkOnAir.jucer
        mv ../overlay-audio-unit/JUCE .
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

s3_path="s3_upload/$TRAVIS_BRANCH/$version_t/$BUILD_OS"
mkdir -p $s3_path

if [ "$BUILD_OS" == "linuxjack" ]; then
    ./studio-link-standalone -t
    ldd studio-link-standalone

    strip --strip-all studio-link-standalone

    chmod +x studio-link-standalone
    mv studio-link-standalone studio-link-standalone-$version_tc
    tar -cvzf studio-link-standalone-$version_tc.tar.gz studio-link-standalone-$version_tc

    cp -a studio-link-standalone-$version_tc.tar.gz $s3_path
fi

if [ "$BUILD_OS" == "linux" ]; then
    ./studio-link-standalone -t
    ldd studio-link-standalone

    strip --strip-all studio-link-standalone
    strip --strip-all overlay-lv2/studio-link.so
    strip --strip-all overlay-onair-lv2/studio-link-onair.so

    chmod +x studio-link-standalone 
    mv studio-link-standalone studio-link-standalone-$version_tc
    tar -cvzf studio-link-standalone-$version_tc.tar.gz studio-link-standalone-$version_tc

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

    cp -a studio-link-standalone-$version_tc.tar.gz $s3_path
    cp -a studio-link-plugin-linux.zip $s3_path
    cp -a studio-link-plugin-onair-linux.zip $s3_path
fi

if [ "$TRAVIS_OS_NAME" == "osx" ]; then
    cp -a ~/Library/Audio/Plug-Ins/Components/StudioLink.component StudioLink.component
    cp -a ~/Library/Audio/Plug-Ins/Components/StudioLinkOnAir.component StudioLinkOnAir.component
    mv overlay-standalone-osx/build/Release/StudioLinkStandalone.app StudioLinkStandalone.app
    cp -a StudioLinkStandalone.app StudioLinkStandaloneHardened.app
    codesign -f --verbose -s "Developer ID Application: Sebastian Reimers (CX34XZ2JTT)" StudioLinkStandalone.app
    codesign -f --verbose -s "Developer ID Application: Sebastian Reimers (CX34XZ2JTT)" StudioLink.component
    codesign -f --verbose -s "Developer ID Application: Sebastian Reimers (CX34XZ2JTT)" StudioLinkOnAir.component
    codesign --options runtime -f --verbose -s "Developer ID Application: Sebastian Reimers (CX34XZ2JTT)" StudioLinkStandaloneHardened.app
    sed $sed_opt s/ITSR:\ StudioLinkOnAir/StudioLinkOnAir\ \(ITSR\)/ StudioLinkOnAir.component/Contents/Info.plist # Reaper 5.70 Fix
    zip -r studio-link-plugin-osx StudioLink.component
    zip -r studio-link-plugin-onair-osx StudioLinkOnAir.component
    zip -r studio-link-standalone StudioLinkStandalone.app
    zip -r studio-link-standalone-hardened StudioLinkStandaloneHardened.app

    cp -a studio-link-standalone.zip $s3_path/studio-link-standalone-$version_tc.zip
    cp -a studio-link-standalone-hardened.zip $s3_path/studio-link-standalone-hardened-$version_tc.zip
    cp -a studio-link-plugin-osx.zip $s3_path
    cp -a studio-link-plugin-onair-osx.zip $s3_path
fi
