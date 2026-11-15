#!/bin/bash -e
#
# Merge the arm64 and x86_64 StudioLink.app builds into a universal app
#
# Usage: dist/macos_universal.sh <arm64 zip> <x86_64 zip> <output dir>
#
#   zips:   studio-link-standalone-<version>-<arch>.zip from dist/build.sh
#           (BUILD_TARGET macos_arm64/macos_x86_64)
#   output: <output dir>/studio-link-standalone-<version>.zip
#
# Not signed (the slices keep their linker signature), release signing and
# notarization: docs/release.md
# Local universal build: dist/build_macos_universal.sh
#

if [ $# -ne 3 ]; then
    echo "usage: $0 <arm64 zip> <x86_64 zip> <output dir>" >&2
    exit 1
fi

source "$(dirname "$0")/lib/functions.sh"

zip_arm64="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
zip_x86_64="$(cd "$(dirname "$2")" && pwd)/$(basename "$2")"
mkdir -p "$3"
out_dir="$(cd "$3" && pwd)"

name="$(basename "$zip_arm64" -arm64.zip)"
if [ "$name" == "$(basename "$zip_arm64")" ] || \
   [ "$name" != "$(basename "$zip_x86_64" -x86_64.zip)" ]; then
    echo "expected <name>-arm64.zip and <name>-x86_64.zip" >&2
    exit 1
fi

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

ditto -x -k "$zip_arm64" "$tmp/arm64"
ditto -x -k "$zip_x86_64" "$tmp/x86_64"

app=StudioLink.app
bin=Contents/MacOS/StudioLink
plist=Contents/Info.plist

for arch in arm64 x86_64; do
    if [ "$(lipo -archs "$tmp/$arch/$app/$bin")" != "$arch" ]; then
        echo "$arch zip: $app is not a $arch build" >&2
        exit 1
    fi
done

for key in CFBundleIdentifier CFBundleShortVersionString CFBundleVersion; do
    a=$(/usr/libexec/PlistBuddy -c "Print :$key" "$tmp/arm64/$app/$plist")
    b=$(/usr/libexec/PlistBuddy -c "Print :$key" "$tmp/x86_64/$app/$plist")
    if [ "$a" != "$b" ]; then
        echo "$key differs: $a (arm64) != $b (x86_64)" >&2
        exit 1
    fi
done

# everything except the executable and the signature must be the same
diff -r -x _CodeSignature -x MacOS \
    "$tmp/arm64/$app" "$tmp/x86_64/$app"

mkdir -p "$tmp/universal"
cp -a "$tmp/arm64/$app" "$tmp/universal/"
lipo -create -output "$tmp/universal/$app/$bin" \
    "$tmp/arm64/$app/$bin" "$tmp/x86_64/$app/$bin"
lipo -info "$tmp/universal/$app/$bin"

sl_macos_zip "$tmp/universal/$app" "$out_dir/$name.zip"

echo "universal app: $out_dir/$name.zip"
