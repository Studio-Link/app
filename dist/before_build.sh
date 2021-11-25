#!/bin/bash -e

if [ "$BUILD_OS" == "linux" ]; then
        sudo apt-get update
        sudo apt-get install -y libasound2-dev libjack-jackd2-dev libpulse-dev libpulse0

        if [ "$BUILD_TARGET" == "linux_arm32" ] || [ "$BUILD_TARGET" == "linux_arm64" ]; then
            sudo apt-get install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu \
                gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf

            wget http://ports.ubuntu.com/pool/main/a/alsa-lib/libasound2_1.1.3-5_armhf.deb
            wget http://ports.ubuntu.com/pool/main/a/alsa-lib/libasound2_1.1.3-5_arm64.deb

            wget http://ports.ubuntu.com/pool/main/p/pulseaudio/libpulse0_11.1-1ubuntu7_armhf.deb
            wget http://ports.ubuntu.com/pool/main/p/pulseaudio/libpulse0_11.1-1ubuntu7_arm64.deb

            sudo dpkg-deb -x libasound2_1.1.3-5_armhf.deb /
            sudo dpkg-deb -x libasound2_1.1.3-5_arm64.deb /
            sudo dpkg-deb -x libpulse0_11.1-1ubuntu7_armhf.deb /
            sudo dpkg-deb -x libpulse0_11.1-1ubuntu7_arm64.deb /
            pushd /usr/lib/arm-linux-gnueabihf
            sudo ln -s libasound.so.2.0.0 libasound.so
            sudo ln -s libpulse-simple.so.0.1.1 libpulse-simple.so
            sudo ln -s libpulse.so.0.20.2 libpulse.so
            popd
            pushd /usr/lib/aarch64-linux-gnu
            sudo ln -s libasound.so.2.0.0 libasound.so
            sudo ln -s libpulse-simple.so.0.1.1 libpulse-simple.so
            sudo ln -s libpulse.so.0.20.2 libpulse.so
            popd
        else

            wget http://lv2plug.in/spec/lv2-1.14.0.tar.bz2
            tar xjf lv2-1.14.0.tar.bz2 
            pushd lv2-1.14.0 && ./waf configure && ./waf build && sudo ./waf install && popd
        fi
elif [ "$BUILD_OS" == "macos" ]; then
        security create-keychain -p travis sl-build.keychain
        security list-keychains -s ~/Library/Keychains/sl-build.keychain
        security unlock-keychain -p travis ~/Library/Keychains/sl-build.keychain
        security set-keychain-settings ~/Library/Keychains/sl-build.keychain
        #security import ./dist/keychain/apple.cer -k ~/Library/Keychains/sl-build.keychain -A
        #security import ./dist/keychain/cert.cer -k ~/Library/Keychains/sl-build.keychain -A
        security import ./dist/keychain/cert_2020.p12 -k ~/Library/Keychains/sl-build.keychain -P $KEY_PASSWORD_2020 -A
        security set-key-partition-list -S apple-tool:,apple:,codesign: -s -k travis sl-build.keychain
fi
