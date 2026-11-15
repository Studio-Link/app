# repository root (functions are called from build/ and from dist/)
sl_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

sl_prepare_version() {
    vminor_t=$(printf "%02d" $vminor)
    version_t="v$vmajor.$vminor_t.$vpatch-$release"
    version_tc="v$vmajor.$vminor_t.$vpatch"
}

sl_prepare() {
    echo "start build on $BUILD_TARGET ($BUILD_OS)"

    sl_prepare_version

    mkdir -p build;
    pushd build

    # release files of this target, uploaded by CI (actions/upload-artifact)
    artifacts="artifacts/$BUILD_TARGET"
    mkdir -p $artifacts
}

sl_3rdparty() {
    #Get 3rdparty prebuilds
    if [ -f ../../3rdparty/build/$BUILD_TARGET.zip ]; then
        cp ../../3rdparty/build/$BUILD_TARGET.zip $BUILD_TARGET.zip
    else
        wget https://github.com/Studio-Link/3rdparty/releases/download/${sl3rdparty}/$BUILD_TARGET.zip
    fi
    unzip -qo $BUILD_TARGET.zip
}

sl_get_webui() {
    # CI: webui.zip is provided by the webui job (download-artifact),
    # otherwise build it locally
    if [ -f webui.zip ]; then
        unzip -o webui.zip
    else
        sl_build_webui
    fi
    mkdir -p ../src/modules/webapp/assets
    cp -a webui/headers/*.h ../src/modules/webapp/assets/
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
    wget -q "https://github.com/baresip/re/archive/refs/tags/v${re}.tar.gz" \
        -O re-${re}.tar.gz
    tar --no-same-owner -xzf re-${re}.tar.gz
    ln -sfn re-$re re
}

sl_get_baresip() {
    wget -q "https://github.com/baresip/baresip/archive/refs/tags/v${baresip}.tar.gz" \
        -O baresip-${baresip}.tar.gz
    tar --no-same-owner -xzf baresip-${baresip}.tar.gz
    ln -sfn baresip-$baresip baresip
    pushd baresip
    patch -p1 < ../../dist/patches/baresip_${baresip}_dtls_aes256.patch
    patch -p1 < ../../dist/patches/baresip_${baresip}_ausrc_nonblocking.patch
    popd
}

# sl_cmake_variant <variant> [extra cmake args]
# Builds re + baresip + Studio Link modules into build/<variant>, or into
# build/$SL_VARIANT_DIR if set
sl_cmake_variant() {
    local variant=$1; shift
    cmake -S .. -B ${SL_VARIANT_DIR:-$variant} \
        -DCMAKE_BUILD_TYPE=$build_type \
        -DSL_VARIANT=$variant \
        -DSL_VERSION=$version_t \
        -DSL_3RDPARTY_DIR=$(pwd)/3rdparty \
        -DSL_RE_DIR=$(pwd)/re \
        -DSL_BARESIP_DIR=$(pwd)/baresip \
        -DSL_CLAP_WRAPPER_VERSION=$clap_wrapper \
        -DSL_CACERT_VERSION=$cacert \
        -DSL_CACERT_SHA256=$cacert_sha256 \
        "$@"
    cmake --build ${SL_VARIANT_DIR:-$variant} -j ${JOBS:-4}
}

# sl_macos_zip <app> <zip>
# ditto keeps the bundle structure/symlinks intact (unlike zip -r)
sl_macos_zip() {
    rm -f "$2"
    ditto -c -k --sequesterRsrc --keepParent "$1" "$2"
}
