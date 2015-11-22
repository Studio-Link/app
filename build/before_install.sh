#!/bin/bash -ex

if [ "$1" == "linux" ]; then
    if [ "$(id -u)" == "0" ]; then
        apt-get update -qq
        apt-get install -y libssl-dev
        wget http://lv2plug.in/spec/lv2-1.12.0.tar.bz2
        tar xjf lv2-1.12.0.tar.bz2 
        pushd lv2-1.12.0 && ./waf configure && ./waf build && sudo ./waf install && popd

        # remove libs to prefer static binding (*.a)
        rm -f /usr/lib/x86_64-linux-gnu/libcrypto.so
        rm -f /usr/lib/x86_64-linux-gnu/libssl.so
    fi
else
    if [ "$(id -u)" != "0" ]; then
        brew update
        brew install openssl
    fi
fi
