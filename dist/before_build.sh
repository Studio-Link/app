#!/bin/bash -e

if [ "$BUILD_OS" == "linux" ]; then
    if [ "$(id -u)" == "0" ]; then
        apt-get update -qq
        apt-get install -y libasound2-dev libjack-jackd2-dev libpulse-dev libpulse0 xxd
        wget http://lv2plug.in/spec/lv2-1.14.0.tar.bz2
        tar xjf lv2-1.14.0.tar.bz2 
        pushd lv2-1.14.0 && ./waf configure && ./waf build && sudo ./waf install && popd
    fi
elif [ "$BUILD_OS" == "osx" ]; then
    if [ "$(id -u)" != "0" ] && [ "$CIRCLECI" != "true" ]; then
#        brew update
#        brew install openssl
        security create-keychain -p travis sl-build.keychain
        security import ./dist/keychain/apple.cer -k ~/Library/Keychains/sl-build.keychain -T /usr/bin/codesign
        security import ./dist/keychain/cert.cer -k ~/Library/Keychains/sl-build.keychain -T /usr/bin/codesign
        security import ./dist/keychain/key.p12 -k ~/Library/Keychains/sl-build.keychain -P $KEY_PASSWORD -T /usr/bin/codesign
        # Increase default timeout 5min
        security set-keychain-settings -t 3600 -l ~/Library/Keychains/sl-build.keychain
    fi
fi
