# Changelog

## [v20.02.0-rc1]

### Fixed

- **Standalone** Output Mono Support (needed by some bluetooth headsets)

### Changed

- **Standalone** Auto-Record disabled


## [v20.02.0-rc0] - 2020-02-04

### Added

- **Standalone** [#100] Auto-Record, starts/end with active call

### Changed

- **Standalone** Increase record audio buffer

### Fixed

- **Misc** Crashes caused by logging and DNS refresh (not thread safe)
- **Misc** fix sysroot windows - and pthread detection for audio_txmode


## [v20.01.3-beta] - 2020-02-03

### Changed

- **Standalone** Open record folder on stop too
- **Misc** Uses baresip audio_txmode = thread now

### Fixed

- **Standalone** Prevent resampling crashes with >48kHz
- **Standalone** Better logging
- **Plugin** Fixes G722 and slogging


## v20.01.2-beta - 2020-01-29

### Fixed

- **Standalone** Add macOS microphone description


## [v20.01.1-beta] - 2020-01-17

### Fixed

- **Standalone** Try to fix crashes by adding entitlements.plist


## [v20.01.0-beta] - 2020-01-12

### Added

- **Standalone** Option to mix input channels
- **Misc** Update check
- **Standalone** Experimental ASIO support on Windows
- **Standalone** Experimental JACK support on Linux
- **Plugin** Linux VST
- **Misc** Implement Remote Logging - [Privacy Policy](https://studio-link.de/datenschutz_en.html)

### Changed

- **Standalone** only start separate RTAUDIO instances if samplerate mismatches.
  Linux and Windows only (macOS produces buffer underruns)
- **Misc** Prefer 1.1.1.1 nameserver
- **Misc** Flac 1.3.3

### Removed

- **Misc** 32 bit Support

### Fixed

- **Standalone** [#87] Web-UI non functional / blocks browser on Raspbian
  Thanks @BayCom
- **Misc** [#53] Wrong Call audio level allocation


# Syntax

## Types

- **Added** for new features
- **Changed** for changes in existing functionality
- **Deprecated** for soon-to-be removed features
- **Removed** for now removed features
- **Fixed** for any bug fixes
- **Security** in case of vulnerabilities

## Labels

- **Standalone** Changes to the standalone OS X, Windows and Linux Version
- **Plugin** Changes to the effect plugins (Audio Unit, LV2 and VST)
- **onAir** Changes to the onAir streaming effect plugin (Audio Unit, LV2 and VST)
- **Box** Changes to the [orange] hardware box
- **Misc** Anything that is left or general.


[Changelog Archive...](https://github.com/Studio-Link/app/blob/v19.xx.x/CHANGELOG-ARCHIVE.md)

[Unreleased]: https://github.com/Studio-Link/app/compare/v20.02.0-rc0...HEAD
[v20.02.0-rc0]: https://github.com/Studio-Link/app/compare/v20.01.3-beta...v20.02.0-rc0
[v20.01.3-beta]: https://github.com/Studio-Link/app/compare/v20.01.1-beta...v20.01.3-beta
[v20.01.1-beta]: https://github.com/Studio-Link/app/compare/v20.01.0-beta...v20.01.1-beta
[v20.01.0-beta]: https://github.com/Studio-Link/app/compare/v19.09.0-beta...v20.01.0-beta
[#87]: https://gitlab.com/studio.link/app/issues/87
[#53]: https://gitlab.com/studio.link/app/issues/53
[#100]: https://gitlab.com/studio.link/app/issues/100
