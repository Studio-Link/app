#!/bin/bash -e
#
# Download the macOS release from GitHub, sign and notarize it
#
# Usage: dist/tools/sign_macos.sh [tag]
#
#   tag:    GitHub release, e.g. v26.11.0-rc2 (default from dist/lib/versions.sh)
#   output: build/release-signed/<tag>/ (same file names as the release)
#
# Prerequisites: docs/release.md
#

source "$(dirname "$0")/../lib/functions.sh"
cd "$sl_root"
source dist/lib/versions.sh
sl_prepare_version

TAG="${1:-${version_t%-"$vbuild"}}"
ID="Developer ID Application: Sebastian Reimers (CX34XZ2JTT)"
APP_ENT="$sl_root/src/standalone/macos/StudioLink.entitlements"
out="$sl_root/build/release-signed/$TAG"

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

notarize() {
    ditto -c -k --keepParent "$1" "$tmp/notarize.zip"
    xcrun notarytool submit "$tmp/notarize.zip" \
        --keychain-profile studio-link --wait | tee "$tmp/notarize.log"
    # on "Invalid": xcrun notarytool log <submission-id> --keychain-profile studio-link
    grep -q "status: Accepted" "$tmp/notarize.log"
    rm "$tmp/notarize.zip"
}

echo "=== download $TAG"
mkdir -p "$tmp/dl"
# asset URLs (the standalone zip name contains the CI build hash)
urls=$(curl -fsSL "https://api.github.com/repos/Studio-Link/app/releases/tags/$TAG" |
    grep -o 'https://[^"]*-macos_universal\.zip')
for url in $urls; do
    curl -fL --output-dir "$tmp/dl" -O "$url"
done
rm -rf "$out"
mkdir -p "$out"

for zip in "$tmp"/dl/*.zip; do
    name="$(basename "$zip")"
    dir="$tmp/${name%.zip}"
    echo "=== $name"

    ditto -x -k "$zip" "$dir"
    xattr -dr com.apple.quarantine "$dir"

    if [ -d "$dir/StudioLink.app" ]; then
        app="$dir/StudioLink.app"
        lipo -info "$app/Contents/MacOS/StudioLink"
        strip "$app/Contents/MacOS/StudioLink"
        codesign --force --options runtime --timestamp \
            --entitlements "$APP_ENT" --sign "$ID" "$app"
        codesign --verify --strict --deep --verbose=2 "$app"
        # without audio-input the microphone is blocked silently
        codesign --display --entitlements - "$app" | grep -q audio-input

        notarize "$app"
        xcrun stapler staple "$app"
        spctl --assess --type execute --verbose=4 "$app"
        sl_macos_zip "$app" "$out/$name"
    else
        # DAW plugins (CLAP, VST3, AUv2), no entitlements
        for b in "$dir"/*; do
            [ -d "$b" ] && lipo -info "$b"/Contents/MacOS/*
            codesign --force --options runtime --timestamp --sign "$ID" "$b"
            codesign --verify --strict --verbose=2 "$b"
        done

        notarize "$dir"
        for b in "$dir"/*; do
            # a plain-file .clap can't be stapled (checked online instead)
            xcrun stapler staple "$b" || [[ "$b" == *.clap ]]
        done
        (cd "$dir" && ditto -c -k --sequesterRsrc . "$out/$name")
    fi
done

echo "=== signed: $out"
ls -l "$out"
