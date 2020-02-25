# Changelog Archive

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


## [v19.09.0-beta] - 2019-09-17

- **Misc** Baresip 0.6.3
- **Misc** OpenSSL 1.1.1c
- **Standalone** Automatic samplerate and channel conversion


## v19.04.0-beta - 2019-04-20

- **Standalone** Onboarding is disabled after first pass or by skipping
- **Standalone** The recordings are now stored in a subfolder
- **Standalone** The Mono/Stereo setting is retained at restart
- **Standalone** UTF-8 support for macOS audio devices


## v19.03.1-alpha (04.04.2019)

- **Misc** Cleanup svg fonts - smaller packages
- **Standalone** Fix OnAir segfaults with no other call active
- **Standalone** Old Studio Link Standalone versions didn't work anymore after execution


## v19.03.0-alpha (31.03.2019)

- **Misc** OpenSSL 1.1.1b
- **Misc** Opus 1.3
- **Misc** Baresip 0.6.0
- **Misc** re 0.6.0
- **Misc** rem 0.6.0
- **Misc** #48 - Use 127.0.0.1 instead of localhost as URL opener
- **Misc** #45 - Configure fixed dns resolver list in baresip
- **Misc** Small GUI improvements (new logo etc.)
- **Plugin** Increase call limit 8 -> 10
- **Standalone** Add RtAudio 
- **Standalone** Audio Interface selection
- **Standalone** Multitrack Feature incl. N-1
- **Standalone** Multitrack Recording (max. 5 Remote Tracks)


## v17.03.1-beta (30.03.2017)

- **Standalone** Add session identifier to recording filename


## v17.03.0-beta (23.03.2017)

- **Misc** OpenSSL 1.1.0e
- **Standalone** Flac 1.3.2
- **Plugin** Remove Auto N-1 Mix Option (permanent active now)
- **onAir** Fixed Streaming Bug macOS auval and Logic
- **Plugin** **onAir** Fixed Windows Crash Dumps


## v16.12.1-beta (30.01.2017)

- **Standalone** Record button better stability


## v16.12.0-beta (16.12.2016)

- **Misc** Fix crash bug on windows 10
- **Misc** OpenSSL 1.1.0c
- **Misc** Opus 1.1.3
- **Misc** Add FLAC Support
- **Misc** Update Baresip and re libs
- **Plugin** Fix opus crash after multiple plug-in reloads
- **onAir** First release of the new streaming plug-in
- **Standalone** Add mono button feature
- **Standalone** Add record button feature


## v16.04.1-beta (02.07.2016)

- **Plugin** Increase tracks (6->8)
- **Plugin Linux LV2** Fix sync bug linux
- **Misc** Fix possible failed audio channel (mostly one direction) after 30-300 seconds


## v16.04.0-beta (23.05.2016)

- **Misc** Accept dialing with enter key
- **Misc** Cleanup Webinterface
- **Plugin** Fix another OSX auval crash bug
- **Plugin** Fix bluetooth conflict #5
- **Standalone** OSX fix default samplerate (48kHz)
- **Standalone** Mute Button on active call


## v16.02.2-beta (03.03.2016)

- **Plugin** Fix audio routing sync bug
- **Plugin** Windows/VST: Add version information
- **Misc** OpenSSL 1.0.2g


## v16.02.1-beta (14.02.2016)

- **Plugin** Add track bypass
- **Misc** Rename audio in-/outputs
- **Misc** re 0.4.15


## v16.02.0-beta (07.02.2016)

- **Standalone** Fix Audiounit mic input bug
- **Standalone** Signing OS X app bundle
- **Plugin** Fix Apple Logic auval crashes
- **Plugin** Raise audio channels/calls from 4 to 6
- **Plugin** Add option auto-mix-n-1
- **Misc** Fix possible Ghost Calls (rewrite call id handling)
- **Misc** Support OS X Versions <10.11
- **Misc** Improve vumeter warning state
- **Misc** Show Studio Link SIP ID on front page
- **Misc** Baresip 0.4.17
- **Misc** rem 0.4.7
- **Misc** Opus 1.1.2
- **Misc** OpenSSL 1.0.2f


## v15.12.2-beta (05.01.2016)

- **Standalone** Add OS X App Bundle
- **Plugin** Fix VST crashes on windows at DAW startup


## v15.12.1-beta (31.12.2015)

- **Misc** Add update available notification
- **Misc** Fix vumeter on windows


## v15.12.0-beta (24.12.2015)

- **Misc** Complete webapp rewrite
- **Misc** Multi call handling
- **Plugin** Basic LV2 and Audio Unit Version
- **Standalone** Basic OS X and Linux Version
- **Standalone** Add vumeter
- **Misc** Use Opus 1.1.1
- **Misc** OpenSSL 1.0.2e (Linux)
- **Misc** Baresip 0.4.16

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

[v20.02.0-rc0]: https://github.com/Studio-Link/app/compare/v20.01.3-beta...v20.02.0-rc0
[v20.01.3-beta]: https://github.com/Studio-Link/app/compare/v20.01.1-beta...v20.01.3-beta
[v20.01.1-beta]: https://github.com/Studio-Link/app/compare/v20.01.0-beta...v20.01.1-beta
[v20.01.0-beta]: https://github.com/Studio-Link/app/compare/v19.09.0-beta...v20.01.0-beta
[#87]: https://gitlab.com/studio.link/app/issues/87
[#53]: https://gitlab.com/studio.link/app/issues/53
[#100]: https://gitlab.com/studio.link/app/issues/100
[v19.09.0-beta]: https://github.com/Studio-Link/app/compare/v19.04.0-beta-605.7ebfed8...v19.09.0-beta
