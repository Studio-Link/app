# Studio Link

<div align="center">
	<a href="https://studio-link.de" target="_blank">
		<img src="https://studio-link.de/assets/webbanner/sl_button-234x60@3x.png" width="500">
	</a>
</div>
<div align="center">
<a href="https://travis-ci.org/Studio-Link/app"><img src="https://travis-ci.org/Studio-Link/app.svg?branch=v19.xx.x" alt="Build Status"></a>
</div>

## About Studio Link

Studio Link is a SIP application to create high quality Audio over IP (AoIP) connections.

## Usage

Please use the prebuilt binarys at https://doku.studio-link.de/standalone/installation-standalone.html

## Supported Platforms

- Windows 64Bit
- macOS 64Bit
- Linux 64Bit

## Changelog

See [CHANGELOG.md](CHANGELOG.md)


## Development

### CI builds

Every CI run uploads the builds as artifacts (`studio-link-<target>`), see
the [Actions](https://github.com/Studio-Link/app/actions) tab.

### Build Requirements

- CMake (>= 3.18), a C compiler, zip/unzip
- Header files for ALSA, PulseAudio and JACK
- xxd and nodejs/npm (only for building the webui)

On Ubuntu/Debian:

    sudo apt-get install cmake build-essential zip unzip \
        libasound2-dev libpulse-dev libjack-jackd2-dev xxd

All other libraries (re/baresip 4.x, OpenSSL, Opus, FLAC, libsamplerate and
PortAudio) are built and linked statically. The Linux binary only depends on
glibc, libpulse (and libjack for the `linuxjack` target). ALSA is loaded at
runtime.

Useful options for `dist/build.sh`: `BUILD_TYPE=Debug`, `SL_SKIP_WEBUI=1`
(use existing `src/modules/webapp/assets`), `SL_SKIP_PLUGINS=1` (don't build
the DAW plugins).

### DAW plugins

The plugins are built from `src/plugin` for the `plugin` and `onair`
variants with `-DSL_PLUGIN_FORMATS="CLAP;VST3;AUV2;LV2"`:

- CLAP (`src/plugin/clap/sl_clap.c`), VST3 and AUv2 are generated from it by
  [clap-wrapper](https://github.com/free-audio/clap-wrapper), which downloads
  the CLAP, VST3 and AudioUnit SDKs at configure time
- LV2 (`src/plugin/lv2/sl_lv2.c`)

`dist/build.sh` builds CLAP/VST3/LV2 on Linux and CLAP/VST3/AUv2 on macOS,
the results are in `build/plugin/plugins` and `build/onair/plugins`.

### CA certificates

TLS (HTTPS provisioning, SIP TLS) uses the Mozilla CA bundle from
[curl](https://curl.se/ca/cacert.pem), embedded into the binaries
(`src/core/slcacert.c`). The bundle version and SHA256 are pinned in
`dist/lib/versions.sh` (`SL_CACERT_VERSION`/`SL_CACERT_SHA256`), CMake
downloads it at configure time. For offline builds use
`-DSL_CACERT_FILE=/path/to/cacert.pem`.

### Build on Linux

```bash
mkdir studio-link
cd studio-link
git clone https://github.com/Studio-Link/3rdparty.git
cd 3rdparty
export BUILD_OS="linux"; export BUILD_TARGET="linux"; dist/build.sh
cd ..
git clone https://github.com/Studio-Link/app.git
cd app
export BUILD_OS="linux"; export BUILD_TARGET="linux"; dist/build.sh
```

### Build on Linux with JACK support

```bash
mkdir studio-link
cd studio-link
git clone https://github.com/Studio-Link/3rdparty.git
cd 3rdparty
export BUILD_OS="linux"; export BUILD_TARGET="linuxjack"; dist/build.sh
cd ..
git clone https://github.com/Studio-Link/app.git
cd app
export BUILD_OS="linux"; export BUILD_TARGET="linuxjack"; dist/build.sh
```

### Build on macOS

```bash
mkdir studio-link
cd studio-link
git clone https://github.com/Studio-Link/3rdparty.git
cd 3rdparty
export BUILD_OS="macos"; export BUILD_TARGET="macos_x86_64"; dist/build.sh
cd ..
git clone https://github.com/Studio-Link/app.git
cd app
export BUILD_OS="macos"; export BUILD_TARGET="macos_x86_64"; dist/build.sh
```

Use `BUILD_TARGET="macos_arm64"` for Apple Silicon. The standalone is a
Swift/AppKit app bundle (`build/StudioLink.app`, macOS 11 or newer) from
`src/standalone/macos`, it needs Xcode (or the Command Line Tools) and
[Ninja](https://ninja-build.org) (`brew install ninja`). Builds are not
signed, releases are signed and notarized manually, see
[docs/release.md](docs/release.md).

The 3rdparty libraries are built per architecture, so `dist/build.sh`
builds a single architecture app
(`build/artifacts/<target>/studio-link-standalone-<version>-<arch>.zip`).
The universal app (arm64 + x86_64) is merged with `lipo`:

```bash
dist/build_macos_universal.sh   # builds both, needs the 3rdparty zips of both
# or merge existing builds (as CI does in the macos_universal job):
dist/macos_universal.sh <arm64 zip> <x86_64 zip> build/artifacts/macos_universal
```

Command line options work with the binary inside the bundle, e.g.
`build/StudioLink.app/Contents/MacOS/StudioLink -v`.

### Build for Windows on Arch Linux (only)

```bash
mkdir studio-link
cd studio-link
git clone https://github.com/Studio-Link/3rdparty.git
cd 3rdparty
export BUILD_OS="mingw"; export BUILD_TARGET="windows64"; dist/build.sh
cd ..
git clone https://github.com/Studio-Link/app.git
cd app
export BUILD_OS="mingw"; export BUILD_TARGET="windows64"; dist/build_windows.sh
```

## License

The Studio Link Apps are open-sourced software licensed under the [MIT license](LICENSE).
