# Release: macOS signing and notarization

CI (and `dist/build.sh`) does not sign anything. The builds are only
"linker signed" (ad-hoc signature of the executable, enough to run them
locally on Apple Silicon). For a release, the app and the plugins are signed
with a Developer ID and notarized by hand on a Mac.

## Prerequisites (once)

- Xcode or the Command Line Tools (`xcode-select --install`)
- A **Developer ID Application** certificate (with private key) in the login
  keychain. Check with:

  ```bash
  security find-identity -v -p codesigning
  # 1) ... "Developer ID Application: <Name> (<TEAMID>)"
  ```

- notarytool credentials, stored in the keychain as profile `studio-link`
  (uses an app-specific password from https://appleid.apple.com):

  ```bash
  xcrun notarytool store-credentials studio-link \
      --apple-id <apple-id-email> --team-id <TEAMID> \
      --password <app-specific-password>
  ```

Used below:

```bash
ID="Developer ID Application: <Name> (<TEAMID>)"
APP_ENT=src/standalone/macos/StudioLink.entitlements   # from the app repo
```

## 1. Get the builds

From the CI run (Actions > run > Artifacts):

- `studio-link-macos_universal`: `studio-link-standalone-<version>.zip`
  (universal StudioLink.app)
- `studio-link-macos_arm64`, `studio-link-macos_x86_64`:
  `studio-link-plugin.zip`, `studio-link-plugin-onair.zip` (per architecture)

Or build locally: `dist/build_macos_universal.sh` (result in
`build/artifacts/`).

Unpack with `ditto` (keeps the bundle structure), into an empty directory:

```bash
mkdir release && cd release
ditto -x -k ../studio-link-standalone-<version>.zip .
# downloaded from GitHub: remove the quarantine flag
xattr -dr com.apple.quarantine StudioLink.app
lipo -info StudioLink.app/Contents/MacOS/StudioLink   # x86_64 arm64
```

## 2. Standalone app (StudioLink.app)

Strip (CI keeps the symbols, the signature is replaced anyway), then sign
with hardened runtime, secure timestamp and the microphone entitlement:

```bash
strip StudioLink.app/Contents/MacOS/StudioLink

codesign --force --options runtime --timestamp \
    --entitlements "$APP_ENT" --sign "$ID" StudioLink.app

codesign --verify --strict --deep --verbose=2 StudioLink.app
codesign --display --entitlements - StudioLink.app
# must show com.apple.security.device.audio-input = true
```

Without the `audio-input` entitlement the hardened runtime blocks the
microphone silently (no permission prompt, the guests hear nothing).

Notarize and staple:

```bash
ditto -c -k --keepParent StudioLink.app StudioLink-notarize.zip
xcrun notarytool submit StudioLink-notarize.zip \
    --keychain-profile studio-link --wait
# on "Invalid": xcrun notarytool log <submission-id> --keychain-profile studio-link

xcrun stapler staple StudioLink.app
xcrun stapler validate StudioLink.app
spctl --assess --type execute --verbose=4 StudioLink.app
# StudioLink.app: accepted, source=Notarized Developer ID
```

Release zip (after stapling, so the ticket is inside):

```bash
rm StudioLink-notarize.zip
ditto -c -k --keepParent StudioLink.app studio-link-standalone-<version>.zip
```

## 3. DAW plugins (CLAP, VST3, AUv2)

Per architecture zip (`studio-link-plugin.zip`: `StudioLink.clap`,
`StudioLink.vst3`, `StudioLink.component`; `studio-link-plugin-onair.zip`:
`StudioLinkOnAir.*`). The plugins need no entitlements (the microphone
permission belongs to the DAW).

```bash
mkdir plugin && ditto -x -k ../studio-link-plugin.zip plugin
xattr -dr com.apple.quarantine plugin

for b in plugin/*.clap plugin/*.vst3 plugin/*.component; do
    codesign --force --options runtime --timestamp --sign "$ID" "$b"
    codesign --verify --strict --verbose=2 "$b"
done

ditto -c -k --keepParent plugin plugin-notarize.zip
xcrun notarytool submit plugin-notarize.zip \
    --keychain-profile studio-link --wait

for b in plugin/*.clap plugin/*.vst3 plugin/*.component; do
    xcrun stapler staple "$b"
done

rm plugin-notarize.zip
(cd plugin && ditto -c -k . ../../studio-link-plugin-<arch>.zip)
```

Same for `studio-link-plugin-onair.zip`. Notes:

- `stapler` cannot staple a `.clap` that is a plain file (not a bundle);
  then it is still notarized online (Gatekeeper checks on first load), the
  error can be ignored.
- After installing, test the AU: `auval -v aufx Ypss ITSR` (Studio Link)
  and `auval -v aufx Eq2z ITSR` (OnAir).

## 4. Check on a clean machine

- Download the release zips through a browser (so they get quarantined),
  unpack with Finder, start StudioLink.app: no Gatekeeper warning
- Microphone prompt appears, guests hear the microphone
- Plugins load in a DAW (Logic for AUv2, Reaper/Bitwig for CLAP/VST3)
