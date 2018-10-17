# Backend Repository

[![Build Status](https://travis-ci.org/Studio-Link/app.svg?branch=v18.xx.x)](https://travis-ci.org/Studio-Link/app)

This repository contains the studio link - baresip modules and build environment

## Build Requirements

- OpenSSL
- Libtool
- LV2 (optional)
- Header files for OpenSSL, ALSA and JACK

### Build Requirements on Ubuntu 16.04

The needed header files are in these packages:  
libssl-dev libasound2-dev libjack-jackd2-dev libtool build-essential

---

## Build on Linux

```export TRAVIS_OS_NAME="linux"; build/build.sh```

## Build on macOS

```export TRAVIS_OS_NAME="osx"; build/build.sh```

## Build for Windows on Archlinux

```export BUILD_OS="windows64"; build/build_windows.sh```
