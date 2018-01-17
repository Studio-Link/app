function sl_prepare {
    echo "start build on $TRAVIS_OS_NAME ($BUILD_OS)"
    vminor_t=$(printf "%02d" $vminor)

    mkdir -p src;
    pushd src
    mkdir -p my_include
}

function sl_get_openssl {
    wget https://www.openssl.org/source/openssl-${openssl}.tar.gz
    echo "$openssl_sha256  openssl-${openssl}.tar.gz" | \
        /usr/bin/core_perl/shasum -a 256 -c -
    tar -xzf openssl-${openssl}.tar.gz
    ln -s openssl-${openssl} openssl
}

