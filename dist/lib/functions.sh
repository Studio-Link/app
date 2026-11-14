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
    unzip -qo $BUILD_TARGET.zip
}

sl_get_webui() {
    s3_path="$GIT_BRANCH/$version_t"
    wget https://download.studio.link/devel/$s3_path/webui.zip
    unzip webui.zip
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
    pushd re
    patch --ignore-whitespace -p1 < ../../dist/patches/re_${re}_studiolink.patch
    popd
}

sl_get_baresip() {
    wget -q "https://github.com/baresip/baresip/archive/refs/tags/v${baresip}.tar.gz" \
        -O baresip-${baresip}.tar.gz
    tar --no-same-owner -xzf baresip-${baresip}.tar.gz
    ln -sfn baresip-$baresip baresip
    pushd baresip
    patch -p1 < ../../dist/patches/baresip_${baresip}_dtls_aes256.patch
    popd
}

# sl_cmake_variant <variant> [extra cmake args]
# Builds re + baresip + Studio Link modules into build/<variant>
sl_cmake_variant() {
    local variant=$1; shift
    cmake -S .. -B $variant \
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
    cmake --build $variant -j ${JOBS:-4}
}
