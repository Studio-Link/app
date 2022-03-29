#!/bin/bash -ex

source dist/lib/versions.sh
source dist/lib/functions.sh

make_opts="-j4 SYSROOT_ALT=../3rdparty "

if [ "$BUILD_TARGET" == "ccheck" ]; then
    dist/tools/ccheck.sh
    exit 0
fi


# Start build
#-----------------------------------------------------------------------------
sl_prepare
sl_3rdparty

sl_extra_lflags="-L ../opus -L ../my_include "
sl_extra_cflags=""

if [ "$BUILD_TARGET" == "linux_arm32" ]; then
    make_opts+="SYSROOT=/usr/arm-linux-gnueabihf"
    sl_extra_lflags+="-L /usr/lib/arm-linux-gnueabihf "
    export CC=arm-linux-gnueabihf-gcc
    export CXX=arm-linux-gnueabihf-g++

fi

if [ "$BUILD_TARGET" == "linux_arm64" ]; then
    make_opts+="SYSROOT=/usr/aarch64-linux-gnu"
    sl_extra_lflags+="-L ./usr/lib/aarch64-linux-gnu "
    export CC=aarch64-linux-gnu-gcc
    export CXX=aarch64-linux-gnu-g++
fi

if [ "$BUILD_OS" == "macos" ]; then 
use_ssl='USE_OPENSSL="yes" USE_OPENSSL_DTLS="yes" USE_OPENSSL_SRTP="yes"'
else
use_ssl='USE_OPENSSL="yes"'
fi

if [ "$BUILD_OS" == "linux" ]; then
    sl_extra_modules="alsa slaudio "
fi
if [ "$BUILD_TARGET" == "linuxjack" ]; then
    sl_extra_modules+="jack "
fi
if [ "$BUILD_OS" == "macos" ]; then 
    export MACOSX_DEPLOYMENT_TARGET=10.10
    sl_extra_lflags+="-L ../openssl ../openssl/libssl.a ../openssl/libcrypto.a "
    sl_extra_lflags+="-framework SystemConfiguration "
    sl_extra_lflags+="-framework CoreFoundation"
    sl_extra_modules="slaudio "
    sed_opt="-i ''"

    if [ "$BUILD_TARGET" == "macos_arm64" ]; then
        xcode="/Applications/Xcode_12.2.app/Contents/Developer"
        sysroot="$xcode/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
        sudo xcode-select --switch $xcode
        #BUILD_CFLAGS="$CFLAGS -arch arm64 -isysroot $sysroot"
        #BUILD_CXXFLAGS="$CXXFLAGS -arch arm64 -isysroot $sysroot"
        #export CFLAGS=$BUILD_CFLAGS
        #export CXXFLAGS=$BUILD_CXXFLAGS
        #_arch="arm64-apple-darwin"
        sl_extra_cflags+=" -arch arm64 -isysroot $sysroot"
        sl_extra_lflags+=" -arch arm64"
    fi
fi

sl_extra_modules+="g722 slogging dtls_srtp"

# Build libre
#-----------------------------------------------------------------------------
if [ ! -d re-$re ]; then
    sl_get_libre

    # WARNING build releases with RELEASE=1, because otherwise its MEM Debug
    # statements are not THREAD SAFE! on every platform, especilly windows.
    make -C re $make_opts $debug $use_ssl EXTRA_LFLAGS="$sl_extra_lflags" \
        EXTRA_CFLAGS="-I ../my_include/ $sl_extra_cflags" libre.a
    mkdir -p my_include/re
    cp -a re/include/* my_include/re/
fi

# Build librem
#-----------------------------------------------------------------------------
if [ ! -d rem-$rem ]; then
    sl_get_librem
    cd rem
    make $debug EXTRA_CFLAGS="$sl_extra_cflags" EXTRA_LFLAGS="$sl_extra_lflags" librem.a 
    cd ..
fi

# Build baresip with studio link addons
#-----------------------------------------------------------------------------
if [ ! -d baresip-$baresip ]; then
    sl_get_baresip

    pushd baresip-$baresip
    # Standalone
    if [ "$BUILD_TARGET" == "linux" ] || [ "$BUILD_TARGET" == "linuxjack" ] || \
       [ "$BUILD_TARGET" == "linux_arm32" ] || [ "$BUILD_TARGET" == "linux_arm64" ]; then
        make $debug $make_opts $use_ssl  LIBRE_SO=../re LIBREM_PATH=../rem STATIC=1 \
            MODULES="opus stdio ice g711 turn stun uuid auloop webapp $sl_extra_modules" \
            EXTRA_CFLAGS="-I ../my_include $sl_extra_cflags" \
            EXTRA_LFLAGS="$sl_extra_lflags -L ../openssl"

        cp -a baresip ../studio-link-standalone
    fi

    # libbaresip.a without effect plugin
    make $debug $make_opts $use_ssl LIBRE_SO=../re LIBREM_PATH=../rem STATIC=1 \
        MODULES="opus stdio ice g711 turn stun uuid auloop webapp $sl_extra_modules" \
        EXTRA_CFLAGS="-I ../my_include $sl_extra_cflags" \
        EXTRA_LFLAGS="$sl_extra_lflags" libbaresip.a
    cp -a libbaresip.a ../my_include/libbaresip_standalone.a

    # Effectonair Plugin
    make clean EXTRA_CFLAGS="$sl_extra_cflags" 
    make $debug $make_opts $use_ssl LIBRE_SO=../re LIBREM_PATH=../rem STATIC=1 \
        MODULES="opus stdio ice g711 turn stun uuid auloop apponair effectonair slogging g722 dtls_srtp" \
        EXTRA_CFLAGS="-I ../my_include -DSLIVE $sl_extra_cflags" \
        EXTRA_LFLAGS="$sl_extra_lflags" libbaresip.a
    cp -a libbaresip.a ../my_include/libbaresip_onair.a

    # Effect Plugin
    make clean EXTRA_CFLAGS="$sl_extra_cflags"
    make $debug $make_opts $use_ssl LIBRE_SO=../re LIBREM_PATH=../rem STATIC=1 \
        MODULES="opus stdio ice g711 turn stun uuid auloop webapp effect g722 slogging dtls_srtp" \
        EXTRA_CFLAGS="-I ../my_include -DSLPLUGIN $sl_extra_cflags" \
        EXTRA_LFLAGS="$sl_extra_lflags" libbaresip.a

    popd
fi

# Build overlay-lv2 plugin (linux only)
#-----------------------------------------------------------------------------
if [ "$BUILD_TARGET" == "linux" ]; then
    if [ ! -d overlay-lv2 ]; then
        git clone $github_org/overlay-lv2.git overlay-lv2
        cd overlay-lv2; ./build.sh; cd ..
    fi
fi

# Build overlay-onair-lv2 plugin (linux only)
#-----------------------------------------------------------------------------
if [ "$BUILD_TARGET" == "linux" ]; then
    if [ ! -d overlay-onair-lv2 ]; then
        git clone $github_org/overlay-onair-lv2.git overlay-onair-lv2
        cd overlay-onair-lv2; ./build.sh; cd ..
    fi
fi

# Build Reaper Linux overlay-vst
#-----------------------------------------------------------------------------
if [ "$BUILD_TARGET" == "linux" ]; then
    if [ ! -d overlay-vst ]; then
        sl_get_overlay-vst
        pushd overlay-vst
        rm Makefile
        mv Makefile.linux Makefile
        make
        popd
    fi
fi

# Build overlay-audio-unit plugin (osx only)
#-----------------------------------------------------------------------------
if [ "$BUILD_OS" == "macos" ]; then
    if [ ! -d overlay-audio-unit ]; then
        git clone \
            $github_org/overlay-audio-unit.git overlay-audio-unit
        cd overlay-audio-unit
        sed -i '' s/SLVERSION_N/$version_n/ StudioLink/StudioLink.jucer
        wget https://github.com/juce-framework/JUCE/releases/download/$juce/juce-$juce-osx.zip
        unzip juce-$juce-osx.zip

        if [ "$BUILD_TARGET" == "macos_x86_64" ]; then
            echo "VALID_ARCHS = x86_64" >> build.xconfig
        else
            echo "VALID_ARCHS = arm64" >> build.xconfig
        fi

        ./build.sh
        cd ..
    fi
fi

# Build overlay-onair-au plugin (osx only)
#-----------------------------------------------------------------------------
if [ "$BUILD_OS" == "macos" ]; then
    if [ ! -d overlay-onair-au ]; then
        git clone \
            $github_org/overlay-onair-au.git overlay-onair-au
        cd overlay-onair-au
        sed -i '' s/SLVERSION_N/$version_n/ StudioLinkOnAir/StudioLinkOnAir.jucer
        mv ../overlay-audio-unit/JUCE .

        if [ "$BUILD_TARGET" == "macos_x86_64" ]; then
            echo "VALID_ARCHS = x86_64" >> build.xconfig
        else
            echo "VALID_ARCHS = arm64" >> build.xconfig
        fi

        ./build.sh
        cd ..
    fi
fi


# Build standalone app bundle (osx only)
#-----------------------------------------------------------------------------
if [ "$BUILD_OS" == "macos" ]; then
    if [ ! -d overlay-standalone-osx ]; then
        git clone \
            $github_org/overlay-standalone-osx.git overlay-standalone-osx
        cp -a my_include/re overlay-standalone-osx/StudioLinkStandalone/
        cp -a my_include/baresip.h \
            overlay-standalone-osx/StudioLinkStandalone/
        cd overlay-standalone-osx
        sed -i '' s/SLVERSION_N/$version_n/ StudioLinkStandalone/Info.plist

        if [ "$BUILD_TARGET" == "macos_x86_64" ]; then
            echo "VALID_ARCHS = x86_64" >> build.xconfig
        else
            echo "VALID_ARCHS = arm64" >> build.xconfig
        fi

        ./build.sh
        cd ..
    fi
fi


# Testing and prepare release upload
#-----------------------------------------------------------------------------

s3_path="s3_upload/$GIT_BRANCH/$version_t/$BUILD_TARGET"
mkdir -p $s3_path

if [ "$BUILD_TARGET" == "linuxjack" ] || \
   [ "$BUILD_TARGET" == "linux_arm32" ] || \
   [ "$BUILD_TARGET" == "linux_arm64" ]; then
    chmod +x studio-link-standalone
    mv studio-link-standalone studio-link-standalone-$version_tc
    tar -cvzf studio-link-standalone-$version_tc.tar.gz studio-link-standalone-$version_tc

    cp -a studio-link-standalone-$version_tc.tar.gz $s3_path
fi

if [ "$BUILD_TARGET" == "linux" ]; then
    #./studio-link-standalone -t 5
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
    zip -r studio-link-plugin studio-link.lv2

    mkdir -p studio-link-onair.lv2
    cp -a overlay-onair-lv2/studio-link-onair.so studio-link-onair.lv2/
    cp -a overlay-onair-lv2/*.ttl studio-link-onair.lv2/
    cp -a overlay-onair-lv2/README.md studio-link-onair.lv2/
    zip -r studio-link-plugin-onair studio-link-onair.lv2

    mkdir -p vst
    pushd vst
    cp ../overlay-vst/studio-link.so .
    zip studio-link-plugin.zip studio-link.so
    rm studio-link.so
    popd

    cp -a studio-link-standalone-$version_tc.tar.gz $s3_path
    cp -a studio-link-plugin.zip $s3_path
    cp -a studio-link-plugin-onair.zip $s3_path
    cp -a vst $s3_path
fi

if [ "$BUILD_OS" == "macos" ]; then
    cp -a ~/Library/Audio/Plug-Ins/Components/StudioLink.component StudioLink.component
    cp -a ~/Library/Audio/Plug-Ins/Components/StudioLinkOnAir.component StudioLinkOnAir.component
    mv overlay-standalone-osx/build/Release/StudioLinkStandalone.app StudioLinkStandalone.app

    mkdir hardened
    cp -a StudioLinkStandalone.app hardened

    codesign -f --verbose -s "Developer ID Application: Sebastian Reimers (CX34XZ2JTT)" StudioLinkStandalone.app
    codesign -f --verbose -s "Developer ID Application: Sebastian Reimers (CX34XZ2JTT)" StudioLink.component
    codesign -f --verbose -s "Developer ID Application: Sebastian Reimers (CX34XZ2JTT)" StudioLinkOnAir.component

    cp -a StudioLink.component hardened
    cp -a StudioLinkOnAir.component hardened

    pushd hardened
    codesign --options runtime --entitlements ../../dist/patches/entitlements.plist -f --verbose -s "Developer ID Application: Sebastian Reimers (CX34XZ2JTT)" StudioLinkStandalone.app
    codesign --display --entitlements - StudioLinkStandalone.app
    popd

    zip -r studio-link-plugin StudioLink.component
    zip -r studio-link-plugin-onair StudioLinkOnAir.component
    zip -r studio-link-standalone StudioLinkStandalone.app

    pushd hardened
    zip -r studio-link-plugin StudioLink.component
    zip -r studio-link-plugin-onair StudioLinkOnAir.component
    zip -r studio-link-standalone-$version_t.zip StudioLinkStandalone.app
    rm -Rf StudioLink.component StudioLinkOnAir.component StudioLinkStandalone.app
    popd

    cp -a studio-link-standalone.zip $s3_path/studio-link-standalone-$version_t.zip
    cp -a studio-link-plugin.zip $s3_path
    cp -a studio-link-plugin-onair.zip $s3_path
    cp -a hardened $s3_path
fi
