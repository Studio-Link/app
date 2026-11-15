#!/bin/bash -e

if [ "$BUILD_OS" == "linux" ]; then
        sudo apt-get update
        sudo apt-get install -y cmake zip unzip \
                libasound2-dev libjack-jackd2-dev libpulse-dev
elif [ "$BUILD_OS" == "macos" ]; then
        # StudioLink.app (Swift) is built with CMake + Ninja
        command -v ninja >/dev/null || brew install ninja
        # no code signing in CI, see docs/release.md
fi
