# Backend Repository

[![Build Status](https://travis-ci.org/Studio-Link/app.svg?branch=v18.xx.x)](https://travis-ci.org/Studio-Link/app)

This repository contains the studio link - baresip modules and build environment

## Build Requirements

- OpenSSL
- Libtool
- LV2 (optional)
- Header files for OpenSSL, ALSA, PulseAudio and JACK
- xxd and nodejs/npm

### Build Requirements on Ubuntu 16.04/18.04

The needed header files are in these packages:  
libssl-dev libasound2-dev libjack-jackd2-dev libtool build-essential 
autoconf automake libpulse0 libpulse-dev xxd

And current nodejs/npm (Node.js v10.x v11.x or v12.x):

https://github.com/nodesource/distributions/blob/master/README.md

---

## Build on Linux

```export TRAVIS_OS_NAME="linux"; dist/build.sh```

## Build on macOS

```export TRAVIS_OS_NAME="osx"; dist/build.sh```

## Build for Windows on Arch Linux (only)

```export BUILD_OS="windows64"; dist/build_windows.sh```
