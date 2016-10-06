#!/bin/bash -e

if [ "$1" == "linux" ]; then
    if [ "$(id -u)" == "0" ]; then
        apt-get update -qq
        apt-get install -y libasound2-dev libjack-jackd2-dev
        wget http://lv2plug.in/spec/lv2-1.14.0.tar.bz2
        tar xjf lv2-1.14.0.tar.bz2 
        pushd lv2-1.14.0 && ./waf configure && ./waf build && sudo ./waf install && popd
    fi
elif [ "$1" == "osx" ]; then
    if [ "$(id -u)" != "0" ]; then
        brew update
        brew install openssl
        security create-keychain -p travis sl-build.keychain
        security import ./build/keychain/apple.cer -k ~/Library/Keychains/sl-build.keychain -T /usr/bin/codesign
        security import ./build/keychain/cert.cer -k ~/Library/Keychains/sl-build.keychain -T /usr/bin/codesign
        security import ./build/keychain/key.p12 -k ~/Library/Keychains/sl-build.keychain -P $KEY_PASSWORD -T /usr/bin/codesign
    fi
fi
