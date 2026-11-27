# Changelog

## [Unreleased]

### Added

- **Standalone/Plugin** G.722 codec (wideband 16 kHz, 64 kbit/s) re-enabled
  as fallback for SIP peers without Opus (e.g. FRITZ!Box, SIP providers),
  not included in OnAir. Fixed
  crackling on loud received audio (decoder output wrapped around instead
  of clipping) and a buffer overflow with oversized RTP packets
- **Misc** Network change detection (baresip netroam module): local
  address/DNS changes (Wi-Fi switch, VPN, resume from sleep) re-register
  accounts without restarting Studio Link
- **Misc** Failed outgoing calls (e.g. "Network is unreachable") are shown
  as notification in the WebUI instead of only in the log
- **Standalone** Recordings only keep the remote-track files of tracks that
  had a call during the recording (no more silent unused tracks)
- **Plugin** New native CLAP, VST3 and AUv2 plugins (via clap-wrapper) and an
  in-tree LV2 plugin, replacing the JUCE and overlay based VST/AU/LV2 builds.
  Linux: CLAP/VST3/LV2, macOS: CLAP/VST3/AUv2, Windows: CLAP/VST3 (AU
  identity is unchanged, so existing DAW sessions keep working)
- **Plugin** Warning in the WebUI if the DAW does not run at 48 kHz
- **Misc** Download logs button (More... > Options > Download logs)
- **Standalone** Windows: runs as a GUI app with a notification area (tray)
  icon instead of a console window. The tray menu opens the web UI, the
  recordings and log folders and quits Studio Link. Starting Studio Link
  again opens the web UI of the running instance. App icon and version info
  added to the .exe
- **Standalone** macOS: new native Swift/AppKit app bundle
  (StudioLink.app, universal arm64/x86_64, macOS 11+) built in-tree,
  replacing the Objective-C overlay-standalone-osx. Menu bar item (click:
  open the web UI, right-click: menu) and Dock menu to open the web UI,
  the recordings and log folders and to quit Studio Link (clean
  shutdown). Clicking the Dock icon or opening the app again opens the web
  UI. Microphone permission is requested at start, startup errors are
  shown in an alert. New app icon (white logo on orange) and menu bar icon
- **Misc** Network status icon per call (green/yellow/red by packet loss,
  round-trip time and jitter). Hover shows debug infos: RTT, bitrate,
  packets, lost packets and jitter (rx/tx), jitter buffer state (delay,
  jitter, skew, lost/late/out-of-sequence/overflow/flush) and audio buffer
  latency. Replaces the "Buffer: x ms" text (baresip patched to keep jitter
  buffer statistics in release builds)

### Changed

- **Misc** Migrated to baresip/libre main, jitter buffering is now handled
  by baresip
- **Misc** Remote logging (log.studio.link) removed, logs are now written
  locally to `<config path>/logs/` (last 7 starts are kept)
- **Misc** TLS uses an embedded Mozilla CA bundle (HTTPS provisioning,
  SIP TLS)
- **Standalone** Audio backend switched from libsoundio to PortAudio
- **Misc** Windows: URLs and folders are opened with ShellExecute instead of
  `system("start ...")` (no cmd window flashing up, also in the plugins)
- **Misc** macOS: the stdio (console keyboard) module is not loaded anymore,
  stdin is /dev/null when started from Finder or inside a DAW
- **Plugin** Reworked audio sync between plugin instances (lock-free ring
  buffer per session)
- **WebUI** Updated to Bootstrap 5 and Vite, new onboarding tour (driver.js),
  jQuery, Vue 2 and bootbox removed
- **Build** New CMake build system, all libraries (re, baresip, OpenSSL,
  Opus, FLAC, libsamplerate, PortAudio) are built and linked statically.
  Most of the old re/baresip patches are no longer needed
- **Build** CI runners updated (Ubuntu 22.04/24.04, macOS 15)

### Removed

- **Misc** Manual per-call latency/buffer buttons (+20ms/-20ms) and the
  "Talk" indicator in active calls
- **Plugin** VST2 plugin
- **Build** 32-bit Windows (windows32) builds, only 64-bit Windows is
  supported

## [v21.07.0-stable] - 2021-07-06

### Fixed

- **Misc** improve accessibility

---

## [v21.05.0-stable] - 2021-06-08

### Added

- **Misc** Volume and Jitter Buffer options

### Changed

- **Misc** Improved jitter handling

### Fixed

- **Standalone** >48kHz Samplerates
- **Misc** IPv6 link local detection

---

## [v21.03.2-stable] - 2021-03-20

### Added

- **Misc** Auto Mono Left/Right/Stereo detection on call start

---

## [v21.03.1-beta] - 2021-03-12

### Added

- **Misc** Mono Left/Right call mix selection

---

## [v21.03.0-beta2] - 2021-03-08

### Fixed

- **Plugin** Close calls on session stop (prevents crash race conditions)

---

## [v21.01.0-beta] - 2021-01-27

### Added

- **Misc** QuickWeb Support

### Fixed

- **Misc** Disable DNS Fallback warnings (debug only now)
- **Misc** Better support/detection for multiple network connections

---

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



[Unreleased]: https://github.com/Studio-Link/app/compare/v21.07.0-stable...HEAD
[v21.07.0-stable]: https://github.com/Studio-Link/app/compare/v21.05.0-stable...v21.07.0-stable
[v21.05.0-stable]: https://github.com/Studio-Link/app/compare/v21.03.2-stable...v21.05.0-stable
[v21.05.0-stable]: https://github.com/Studio-Link/app/compare/v21.03.2-stable...v21.05.0-stable
[v21.03.2-stable]: https://github.com/Studio-Link/app/compare/v21.03.1-beta...v21.03.2-stable
[v21.03.1-beta]: https://github.com/Studio-Link/app/compare/v21.03.0-beta2...v21.03.1-beta
[v21.03.0-beta2]: https://github.com/Studio-Link/app/compare/v21.01.0-beta...v21.03.0-beta2
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
