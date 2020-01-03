# Changelog

## [Unreleased]

### Added

- **Standalone** Option to mix input channels
- **Misc** Update check
- **Standalone** Experimental ASIO support on Windows
- **Standalone** Experimental JACK support on Linux
- **Plugin** Linux VST

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


## [v19.09.0-beta] - 2019-09-17

- **Misc** Baresip 0.6.3
- **Misc** OpenSSL 1.1.1c
- **Standalone** Automatic samplerate and channel conversion


## v19.04.0-beta - 2019-04-20

- **Standalone** Onboarding is disabled after first pass or by skipping
- **Standalone** The recordings are now stored in a subfolder
- **Standalone** The Mono/Stereo setting is retained at restart
- **Standalone** UTF-8 support for macOS audio devices


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

[Unreleased]: https://github.com/Studio-Link/app/compare/v19.09.0-beta...HEAD
[v19.09.0-beta]: https://github.com/Studio-Link/app/compare/v19.04.0-beta-605.7ebfed8...v19.09.0-beta
[#87]: https://gitlab.com/studio.link/app/issues/87
