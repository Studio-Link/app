#!/bin/bash -ex

if [ "$BUILD_OS" == "linux" ]; then
    if [ "$(id -u)" == "0" ]; then
        apt-get update -qq
        apt-get install -y libasound2-dev
        wget http://lv2plug.in/spec/lv2-1.12.0.tar.bz2
        tar xjf lv2-1.12.0.tar.bz2 
        pushd lv2-1.12.0 && ./waf configure && ./waf build && sudo ./waf install && popd
    fi
elif [ "$BUILD_OS" == "osx" ]; then
    if [ "$(id -u)" != "0" ]; then
        brew update
        brew install openssl
    fi
elif [ "$BUILD_OS" == "windows" ]; then
    echo "Not implemented, yet!"
fi
