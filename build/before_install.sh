#!/bin/bash -ex

if [ "$1" == "linux" ]; then
    apt-get update -qq
    apt-get install -y libssl-dev lv2-dev

    # remove libs to prefer static binding (*.a)
    rm -f /usr/lib/x86_64-linux-gnu/libcrypto.so
    rm -f /usr/lib/x86_64-linux-gnu/libssl.so
fi
