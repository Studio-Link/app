sl_prepare_version() {
    vminor_t=$(printf "%02d" $vminor)
    version_t="v$vmajor.$vminor_t.$vpatch-$release"
    version_tc="v$vmajor.$vminor_t.$vpatch"
    version_n="$vmajor.$vminor_t.$vpatch"
}

sl_prepare() {
    echo "start build on $BUILD_TARGET ($BUILD_TARGET)"
    sed_opt="-i"

    sl_prepare_version

    mkdir -p build;
    pushd build
    mkdir -p my_include

    if [ -z "${GITHUB_REF}" ]; then
        GIT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
    else
        GIT_BRANCH=$(echo $GITHUB_REF | awk 'BEGIN { FS = "/" } ; { print $3 }')
    fi

    SHASUM=$(which shasum)
}

sl_3rdparty() {
    #Get 3rdparty prebuilds
    if [ -f ../../3rdparty/build/$BUILD_TARGET.zip ]; then
        cp ../../3rdparty/build/$BUILD_TARGET.zip $BUILD_TARGET.zip
    else
        wget https://github.com/Studio-Link/3rdparty/releases/download/${sl3rdparty}/$BUILD_TARGET.zip
    fi
    unzip $BUILD_TARGET.zip
}

sl_get_webui() {
    s3_path="$GIT_BRANCH/$version_t"
    wget https://download.studio.link/devel/$s3_path/webui.zip
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
    if [ "$BUILD_TARGET" == "linux" ]; then
        pushd overlay-vst/vstsdk2.4
        patch --ignore-whitespace -p1 < ../vst2_linux.patch
        popd
    fi
}

sl_build_webui() {
    cp -a ../src/webui .
    pushd webui
    npm install
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
    libre_url="https://github.com/baresip/re/archive/v"
    wget ${libre_url}${re}.tar.gz -O re-${re}.tar.gz
    tar -xzf re-${re}.tar.gz
    ln -s re-$re re
    pushd re
    patch --ignore-whitespace -p1 < ../../dist/patches/bluetooth_conflict.patch
#    patch --ignore-whitespace -p1 < ../../dist/patches/re_ice_bug.patch
    patch --ignore-whitespace -p1 < ../../dist/patches/re_fix_authorization.patch
    patch --ignore-whitespace -p1 < ../../dist/patches/re_pull_66.diff
#    patch --ignore-whitespace -p1 < ../../dist/patches/re_recv_handler_win_patch.patch
    if [ "$BUILD_TARGET" == "windows32" ] || [ "$BUILD_TARGET" == "windows64" ]; then
        patch -p1 < ../../dist/patches/fix_windows_ssize_t_bug.patch
        #patch -p1 < ../../dist/patches/re_wsapoll.patch
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
    #baresip_url="https://github.com/studio-link-3rdparty/baresip/archive/v"
    baresip_url="https://github.com/baresip/baresip/archive/v"

    wget ${baresip_url}${baresip}.tar.gz -O baresip-${baresip}.tar.gz
    tar -xzf baresip-${baresip}.tar.gz
    ln -s baresip-$baresip baresip
    pushd baresip-$baresip

    ## Add patches
    patch -p1 < ../../dist/patches/config.patch
    patch -p1 < ../../dist/patches/fix_check_telev_and_pthread.patch
    patch -p1 < ../../dist/patches/dtls_aes256.patch
    patch -p1 < ../../dist/patches/rtcp_mux_softphone.patch
    patch -p1 < ../../dist/patches/fallback_dns.patch
    #patch -p1 < ../../dist/patches/osx_sample_rate.patch

    #fixes multiple maxaverage lines in fmtp e.g.: 
    #fmtp: stereo=1;sprop-stereo=1;maxaveragebitrate=64000;maxaveragebitrate=64000;
    #after multiple module reloads it crashes because fmtp is to small(256 chars)
    #patch -p1 < ../../dist/patches/opus_fmtp.patch

    ## Link backend modules
    rm -Rf modules/g722
    ln -s $(pwd -P)/../../src/modules/g722 modules/g722
    ln -s $(pwd -P)/../../src/modules/slogging modules/slogging
    ln -s $(pwd -P)/../../src/modules/webapp modules/webapp
    ln -s $(pwd -P)/../../src/modules/effect modules/effect
    ln -s $(pwd -P)/../../src/modules/effectonair modules/effectonair
    ln -s $(pwd -P)/../../src/modules/apponair modules/apponair
    ln -s $(pwd -P)/../../src/modules/slaudio modules/slaudio

    sed $sed_opt s/SLVERSION_T/$version_t/ modules/webapp/webapp.h
    sed $sed_opt s/BARESIP_VERSION\ \"$baresip_lib\"/BARESIP_VERSION\ \"$version_n\"/ include/baresip.h
    cp -a include/baresip.h ../my_include/
    ln -s $(pwd -P)/../../src/modules/webapp/webapp.h ../my_include/webapp.h

    popd
}
