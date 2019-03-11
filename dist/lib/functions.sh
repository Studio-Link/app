if [ "$CIRCLECI" == "true" ]; then
    function wget() { curl -L "${1}" -o $(basename "${1}") ; };
    curl https://kent.dl.sourceforge.net/project/macpkg/XZ/5.0.7/XZ.pkg -o xz.pkg
    sudo installer -pkg xz.pkg -target /
    brew update
    brew install pkg-config libtool
fi

sl_prepare_version() {
    vminor_t=$(printf "%02d" $vminor)
    version_t="v$vmajor.$vminor_t.$vpatch-$release"
    version_n="$vmajor.$vminor.$vpatch"
}

sl_prepare() {
    echo "start build on $TRAVIS_OS_NAME ($BUILD_OS)"
    sed_opt="-i"

    sl_prepare_version

    mkdir -p build;
    pushd build
    mkdir -p my_include


    SHASUM=$(which shasum)
}

sl_get_openssl() {
    wget https://www.openssl.org/source/openssl-${openssl}.tar.gz
    echo "$openssl_sha256  openssl-${openssl}.tar.gz" | \
        ${SHASUM} -a 256 -c -
    tar -xzf openssl-${openssl}.tar.gz
    ln -s openssl-${openssl} openssl
}

sl_get_flac() {
    wget https://ftp.osuosl.org/pub/xiph/releases/flac/flac-${flac}.tar.xz
    tar -xf flac-${flac}.tar.xz
    ln -s flac-${flac} flac
}

sl_get_rtaudio() {
    wget https://github.com/Studio-Link/rtaudio/archive/${rtaudio}.tar.gz
    tar -xzf ${rtaudio}.tar.gz
    ln -s rtaudio-${rtaudio} rtaudio
    cp -a rtaudio-${rtaudio}/rtaudio_c.h my_include/
    rm -f ${rtaudio}.tar.gz
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
    #wget https://github.com/Studio-Link/baresip/archive/$baresip.tar.gz
    wget "https://github.com/alfredh/baresip/archive/v${baresip}.tar.gz" -O baresip-${baresip}.tar.gz
    tar -xzf baresip-${baresip}.tar.gz
    ln -s baresip-$baresip baresip
    cp -a baresip-$baresip/include/baresip.h my_include/
    pushd baresip-$baresip

    ## Add patches
    patch -p1 < ../../dist/patches/config.patch
    patch -p1 < ../../dist/patches/osx_sample_rate.patch

    #fixes multiple maxaverage lines in fmtp e.g.: 
    #fmtp: stereo=1;sprop-stereo=1;maxaveragebitrate=64000;maxaveragebitrate=64000;
    #after multiple module reloads it crashes because fmtp is to small(256 chars)
    #patch -p1 < ../../dist/patches/opus_fmtp.patch

    ## Link backend modules
    cp -a ../../src/modules/webapp modules/webapp
    cp -a ../../src/modules/effect modules/effect
    cp -a ../../src/modules/effectonair modules/effectonair
    cp -a ../../src/modules/apponair modules/apponair
    cp -a ../../src/modules/slrtaudio modules/slrtaudio

    sed $sed_opt s/SLVERSION_T/$version_t/ modules/webapp/webapp.c
    popd
}
