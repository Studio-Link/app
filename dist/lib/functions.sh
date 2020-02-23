sl_prepare_version() {
    vminor_t=$(printf "%02d" $vminor)
    version_t="v$vmajor.$vminor_t.$vpatch-$release"
    version_tc="v$vmajor.$vminor_t.$vpatch"
    version_n="$vmajor.$vminor.$vpatch"
}

sl_prepare() {
    if [ -z $BUILD_OS ]; then
        export BUILD_OS="$TRAVIS_OS_NAME"
    fi
    echo "start build on $TRAVIS_OS_NAME ($BUILD_OS)"
    sed_opt="-i"

    sl_prepare_version

    mkdir -p build;
    pushd build
    mkdir -p my_include


    SHASUM=$(which shasum)
}

sl_3rdparty() {
    #Get 3rdparty prebuilds
    if [ -f ../../3rdparty/build/$BUILD_OS.zip ]; then
        cp ../../3rdparty/build/$BUILD_OS.zip $BUILD_OS.zip
    else
        wget https://github.com/Studio-Link/3rdparty/releases/download/${sl3rdparty}/$BUILD_OS.zip
    fi
    unzip $BUILD_OS.zip
}

sl_get_webui() {
    s3_path="$TRAVIS_BRANCH/$version_t"
    wget https://s3.eu-central-1.amazonaws.com/studio-link-artifacts/$s3_path/webui.zip
    unzip webui.zip
    mkdir -p ../src/modules/webapp/assets
    cp -a webui/headers/*.h ../src/modules/webapp/assets/
}

sl_get_overlay-vst() {
    git clone https://github.com/Studio-Link/overlay-vst.git
    sed -i s/SLVMAJOR/$vmajor/ overlay-vst/version.h
    sed -i s/SLVMINOR/$vminor/ overlay-vst/version.h
    sed -i s/SLVPATCH/$vpatch/ overlay-vst/version.h
    wget http://www.steinberg.net/sdk_downloads/$vstsdk.zip
    unzip -q $vstsdk.zip
    mv VST_SDK/VST2_SDK overlay-vst/vstsdk2.4
    if [ "$BUILD_OS" == "linux" ]; then
        pushd overlay-vst/vstsdk2.4
        patch --ignore-whitespace -p1 < ../vst2_linux.patch
        popd
    fi
}

sl_build_webui() {
    cp -a ../src/webui .
    pushd webui
    npm ci
    npm run prod
    mkdir -p headers
    xxd -i dist/index.html > headers/index_html.h
    xxd -i dist/app.css > headers/css.h
    xxd -i dist/app.js > headers/js.h
    find dist/images -type f | xargs -I{} xxd -i {} > headers/images.h
    popd
}

sl_get_libre() {
    #wget -N "http://www.creytiv.com/pub/re-${re}.tar.gz"
    wget "https://github.com/creytiv/re/archive/v${re}.tar.gz" -O re-${re}.tar.gz
    tar -xzf re-${re}.tar.gz
    rm -f v${re}.tar.gz
    ln -s re-$re re
    pushd re
    patch --ignore-whitespace -p1 < ../../dist/patches/bluetooth_conflict.patch
    patch --ignore-whitespace -p1 < ../../dist/patches/re_ice_bug.patch
    patch --ignore-whitespace -p1 < ../../dist/patches/re_fix_authorization.patch
    if [ "$BUILD_OS" == "windows32" ] || [ "$BUILD_OS" == "windows64" ]; then
        patch -p1 < ../../dist/patches/fix_windows_ssize_t_bug.patch
    fi
    popd
}

sl_get_librem() {
    #wget -N "http://www.creytiv.com/pub/rem-${rem}.tar.gz"
    wget "https://github.com/creytiv/rem/archive/v${rem}.tar.gz" -O rem-${rem}.tar.gz
    tar -xzf rem-${rem}.tar.gz
    ln -s rem-$rem rem
}

sl_get_baresip() {
    sl_get_webui
    baresip_url="https://github.com/studio-link-3rdparty/baresip/archive/v"
    #baresip_url="https://github.com/alfredh/baresip/archive/v"

    wget ${baresip_url}${baresip}.tar.gz -O baresip-${baresip}.tar.gz
    tar -xzf baresip-${baresip}.tar.gz
    ln -s baresip-$baresip baresip
    pushd baresip-$baresip

    ## Add patches
    #patch -p1 < ../../dist/patches/config.patch
    #patch -p1 < ../../dist/patches/fix_check_telev_and_pthread.patch
    #patch -p1 < ../../dist/patches/osx_sample_rate.patch

    #fixes multiple maxaverage lines in fmtp e.g.: 
    #fmtp: stereo=1;sprop-stereo=1;maxaveragebitrate=64000;maxaveragebitrate=64000;
    #after multiple module reloads it crashes because fmtp is to small(256 chars)
    #patch -p1 < ../../dist/patches/opus_fmtp.patch

    ## Link backend modules
    rm -Rf modules/g722
    cp -a ../../src/modules/g722 modules/g722
    cp -a ../../src/modules/slogging modules/slogging
    cp -a ../../src/modules/webapp modules/webapp
    cp -a ../../src/modules/effect modules/effect
    cp -a ../../src/modules/effectonair modules/effectonair
    cp -a ../../src/modules/apponair modules/apponair
    cp -a ../../src/modules/slrtaudio modules/slrtaudio


    sed $sed_opt s/SLVERSION_T/$version_t/ modules/webapp/webapp.h
    sed $sed_opt s/$baresip/$version_n/ include/baresip.h
    cp -a include/baresip.h ../my_include/

    popd
}
