# Changelog

## [v20.04.2-beta] - 2020-04-xx

### Added

- **Standalone** Mono/Stereo Support (Streaming and Recording)

### Changed

- **Misc** jitter buffer: implement silence counter with 500ms threshold
- **Misc** jitter buffer: reduce max latency optimization from 200ms to 160ms
- **Misc** jitter buffer: display buffer in ms instead of %

### Fixed

- **Standalone** slaudio/flac: fix metadata memory leak
- **Standalone** slaudio: some memory leaks
- **Misc** effect/slaudio: possible crash with new call
- **Misc** slogging: thread safe (libre mqueue) / prevent dns crashes
- **Misc** webui: early websockets bugs (better init handling)
- **Misc** webapp: rewrite webapp.conf parsing
- **Standalone** slaudio: rewrite format conversion


## [v20.04.1-beta1] - 2020-04-22

### Changed

- **Standalone** New Audio Framework (libsoundio)
- **Standalone** Mute-Button repositioned
- **Standalone** Make recording more robust (not rely on system timer anymore)

### Removed

- **Standalone** Windows ASIO Support (will be added later,
please use v20.03.8 until ready)

### Fixed

- **Misc** Audio distortion with low buffer size
and high network jitter (caused by new jitter buffer)

---

## [v20.04.0-beta] - 2020-03-28

### Changed

- **Misc** New jitter buffer implementation

---

## [v20.03.8-stable] - 2020-03-29

### Fixed

- **Misc** Fixes recv handler warnings on windows
- **Standalone** Better max call handling


## [v20.03.7-stable] - 2020-03-22

### Added

- **Misc** DTMF handling

### Fixed

- **Misc** Call hangup handling
- **Standalone** macOS Audiointerface close

---

## [v20.03.6-stable] - 2020-03-20

### Fixed

- **Misc** IE11 Javascript bug (marked removed)

---

## [v20.03.5-stable] - 2020-03-19

### Fixed

- **Misc** Disable prefered DNS resolvers

### Changed

- **Misc** Bump openssl version 1.1.1e

---

## [v20.03.4-stable] - 2020-03-15

### Fixed

- **Standalone** Re-enable OnAir Streaming

### Changed

- **Standalone** Show close warning if a call is active
- **Standalone** Exit Standalone if browser window/tab is closed and no call active
- **Standalone** Re-add Windows 32bit Build

---

## [v20.03.3-stable] - 2020-03-07

### Fixed

- **onAir** Crashes (not necessary modules disabled)
- **Misc** Call crashes with non studio.link SIP accounts

---

## [v20.03.2-stable] - 2020-03-05

### Fixed

- **Standalone** Provisioning Race Condition
- **Misc** Example contacts

---

## [v20.03.1-rc0] - 2020-03-01

Debug only Release

---

## [v20.03.0-rc0] - 2020-03-01

### Added

- **Misc** my.Studio.link support - older clients before this version are not compatible
- **Misc** Full End-to-End Encryption (DTLS-SRTP AES256)
- **Misc** Update Baresip 0.6.5 - dev release

---

## [v20.02.1-rc0] - 2020-02-25

### Added
- **Standalone** Record timer

### Changed

- **Standalone** Auto-Record reverted/disabled
- **Standalone** Open record folder after 5mins
- **Standalone** Consistent track naming (record and active calls view)

### Fixed

- **Standalone** Output Mono Support (needed by some bluetooth headsets)


[Changelog Archive...](https://github.com/Studio-Link/app/blob/v20.xx.x/CHANGELOG-ARCHIVE.md)

---

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



[v20.03.8-stable]: https://github.com/Studio-Link/app/compare/v20.03.7-stable...v20.03.8-stable
[v20.03.7-stable]: https://github.com/Studio-Link/app/compare/v20.03.6-stable...v20.03.7-stable
[v20.03.6-stable]: https://github.com/Studio-Link/app/compare/v20.03.5-stable...v20.03.6-stable
[v20.03.5-stable]: https://github.com/Studio-Link/app/compare/v20.03.4-stable...v20.03.5-stable
[v20.03.4-stable]: https://github.com/Studio-Link/app/compare/v20.03.3-stable...v20.03.4-stable
[Unreleased]: https://github.com/Studio-Link/app/compare/v20.03.3-stable...HEAD
[v20.02.1-rc0]: https://github.com/Studio-Link/app/compare/v20.02.0-rc0...v20.02.1-rc0
