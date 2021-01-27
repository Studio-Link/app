# Changelog

## [Unreleased]

## [v21.01.0-beta] - 2021-01-27

### Added

- **Misc** QuickWeb Support

### Fixed

- **Misc** Disable DNS Fallback warnings (debug only now)
- **Misc** Better support/detection for multiple network connections


## [v20.12.1-stable] - 2021-01-01

### Fixed

- **Misc** Config version

---

## [v20.12.0-stable] - 2021-01-01

### Added

- **Misc** DNS Server Fallback (8.8.8.8)
- **Misc** macOS arm64 (Apple Silicon) beta
- **Standalone** Open record folder button (replaces auto open)

### Changed

- **Misc** baresip 1.0.0 and libre 1.1.0
- **Misc** npm updates
- **Plugin** new thread sync algorithm

### Fixed

- **Misc** webapp/websocket: count ws connections and exit only if all closed
- **Standalone** slaudio: fix stream ch
- **Misc** Fix add contact sip address validation


## [v20.05.5-stable] - 2020-08-27

### Added

- **Misc** Update notification

---

## [v20.05.4-stable] - 2020-08-25 [YANKED]

---

## [v20.05.3-stable] - 2020-08-05

### Fixed

- **Standalone** Windows: Fixed Desktop folder selection on some systems
- **Standalone** Windows: Open record folder uses explicit explorer.exe
- **Standalone** Clear ring buffer, if full

### Changed

- **Misc** g722 Codec disabled (has to be fixed, analyzed)

---

## [v20.05.2-stable] - 2020-06-01

### Fixed

- **Misc** webui: ie11 javascript error

---

## [v20.05.1-stable] [YANKED]

---

## [v20.05.0-stable] - 2020-05-31

### Changed

- **Misc** improve jitter buffer (less latency)

### Fixed

- **Standalone** slaudio: multi streams macOS
- **Standalone** slaudio: possible crash
- **Misc** webapp: config bool read
- **Misc** webapp: gcc 10 linking
- **Plug-in** webui: onboarding and interface 

---

## [v20.04.4-beta] - 2020-05-03

### Added

- **Standalone** slaudio/webapp/ui: audio interface reload button and error message

### Fixed

- **Standalone** slaudio: macOS device switch error


## [v20.04.3-beta] - 2020-05-02

### Fixed

- **Standalone** slaudio: fix mono output
- **Plugin** webapp: fix options 

### Changed

- **Standalone** Disable WASAPI exclusiv devices


## [v20.04.2-beta] - 2020-04-27

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

## Syntax

### Types

- **Added** for new features
- **Changed** for changes in existing functionality
- **Deprecated** for soon-to-be removed features
- **Removed** for now removed features
- **Fixed** for any bug fixes
- **Security** in case of vulnerabilities

### Labels

- **Standalone** Changes to the standalone OS X, Windows and Linux Version
- **Plugin** Changes to the effect plugins (Audio Unit, LV2 and VST)
- **onAir** Changes to the onAir streaming effect plugin (Audio Unit, LV2 and VST)
- **Box** Changes to the [orange] hardware box
- **Misc** Anything that is left or general.



[Unreleased]: https://github.com/Studio-Link/app/compare/v21.01.0-beta...HEAD
[v21.01.0-beta]: https://github.com/Studio-Link/app/compare/v20.12.1-stable...v21.01.0-beta
[v20.12.1-stable]: https://github.com/Studio-Link/app/compare/v20.12.0-stable...v20.12.1-stable
[v20.12.0-stable]: https://github.com/Studio-Link/app/compare/v20.05.5-stable...v20.12.0-stable
[v20.05.5-stable]: https://github.com/Studio-Link/app/compare/v20.05.3...v20.05.5-stable
[v20.05.3-stable]: https://github.com/Studio-Link/app/compare/aa9d2df...v20.05.3
[v20.05.0-stable]: https://github.com/Studio-Link/app/compare/v20.04.4-beta...v20.05.0-stable
[v20.04.4-beta]: https://github.com/Studio-Link/app/compare/v20.04.3-beta...v20.04.4-beta
[v20.04.3-beta]: https://github.com/Studio-Link/app/compare/v20.04.2-beta...v20.04.3-beta
[v20.04.2-beta]: https://github.com/Studio-Link/app/compare/v20.04.1-beta1...v20.04.2-beta
[v20.03.8-stable]: https://github.com/Studio-Link/app/compare/v20.03.7-stable...v20.03.8-stable
[v20.03.7-stable]: https://github.com/Studio-Link/app/compare/v20.03.6-stable...v20.03.7-stable
[v20.03.6-stable]: https://github.com/Studio-Link/app/compare/v20.03.5-stable...v20.03.6-stable
[v20.03.5-stable]: https://github.com/Studio-Link/app/compare/v20.03.4-stable...v20.03.5-stable
[v20.03.4-stable]: https://github.com/Studio-Link/app/compare/v20.03.3-stable...v20.03.4-stable
[v20.02.1-rc0]: https://github.com/Studio-Link/app/compare/v20.02.0-rc0...v20.02.1-rc0
