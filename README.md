# Backend Repository

[![Build Status](https://travis-ci.org/Studio-Link/app.svg?branch=v19.xx.x)](https://travis-ci.org/Studio-Link/app)

This repository contains the studio link - baresip modules and build environment

## Usage

Pleas use the prebuilt binarys at https://doku.studio-link.de/standalone/installation-standalone.html

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

### Build Requirements on macOS 10.14

Install /usr/include Header File with:
/Library/Developer/CommandLineTools/Packages/macOS_SDK_headers_for_macOS_10.14.pkg

---

## Build on Linux

```bash
mdkir studio-link
cd studio-link
git clone https://github.com/Studio-Link/3rdparty.git
cd 3rdparty
export TRAVIS_OS_NAME="linux"; dist/build.sh
cd ..
git clone https://github.com/Studio-Link/app.git
cd app
export TRAVIS_OS_NAME="linux"; dist/build.sh
```

## Build on macOS

```bash
mdkir studio-link
cd studio-link
git clone https://github.com/Studio-Link/3rdparty.git
cd 3rdparty
export TRAVIS_OS_NAME="linux"; dist/build.sh
cd ..
git clone https://github.com/Studio-Link/app.git
cd app
export TRAVIS_OS_NAME="osx"; dist/build.sh
```

## Build for Windows on Arch Linux (only)

```bash
mdkir studio-link
cd studio-link
git clone https://github.com/Studio-Link/3rdparty.git
cd 3rdparty
export TRAVIS_OS_NAME="linux"; dist/build.sh
cd ..
git clone https://github.com/Studio-Link/app.git
cd app
export BUILD_OS="windows64"; dist/build_windows.sh
```
