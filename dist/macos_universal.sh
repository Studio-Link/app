#!/bin/bash -e
#
# Merge the arm64 and x86_64 macOS builds into universal ones
#
# Usage: dist/macos_universal.sh <arm64 dir> <x86_64 dir> <output dir>
#
#   input:  artifact dirs of dist/build.sh (BUILD_TARGET macos_arm64 and
#           macos_x86_64, build/artifacts/<target>) with
#             studio-link-standalone-<version>-<arch>.zip  (StudioLink.app)
#             studio-link-plugin.zip, studio-link-plugin-onair.zip
#                                   (CLAP/VST3/AUv2, missing with
#                                    SL_SKIP_PLUGINS=1)
#   output: <output dir>/studio-link-standalone-<version>.zip
#           <output dir>/studio-link-plugin.zip
#           <output dir>/studio-link-plugin-onair.zip
#
# Every Mach-O file is merged with lipo, all other files must be identical
# in both builds. Not signed (the slices keep their linker signature),
# release signing and notarization: docs/release.md
# Local universal build: dist/build_macos_universal.sh
#

if [ $# -ne 3 ]; then
    echo "usage: $0 <arm64 dir> <x86_64 dir> <output dir>" >&2
    exit 1
fi

source "$(dirname "$0")/lib/functions.sh"

die() {
    echo "$*" >&2
    exit 1
}

dir_arm64="$(cd "$1" && pwd)"
dir_x86_64="$(cd "$2" && pwd)"
mkdir -p "$3"
out_dir="$(cd "$3" && pwd)"

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

# merge_tree <arm64 dir> <x86_64 dir> <output dir>
merge_tree() {
    local a=$1 x=$2 out=$3 f n=0

    # same files, dirs and symlinks
    diff <(cd "$a" && find . | LC_ALL=C sort) \
         <(cd "$x" && find . | LC_ALL=C sort) ||
        die "$(basename "$out"): different files in arm64 and x86_64"

    cp -a "$a" "$out"

    while IFS= read -r -d '' f; do
        case "$f" in
            */_CodeSignature/*) continue ;;
        esac
        if lipo -archs "$a/$f" >/dev/null 2>&1; then
            [ "$(lipo -archs "$a/$f")" == "arm64" ] ||
                die "$f: not an arm64 build ($(lipo -archs "$a/$f"))"
            [ "$(lipo -archs "$x/$f")" == "x86_64" ] ||
                die "$f: not an x86_64 build ($(lipo -archs "$x/$f"))"
            lipo -create -output "$out/$f" "$a/$f" "$x/$f"
            lipo -info "$out/$f"
            n=$((n + 1))
        else
            cmp -s "$a/$f" "$x/$f" ||
                die "$f differs between arm64 and x86_64"
        fi
    done < <(cd "$a" && find . -type f -print0)

    [ $n -gt 0 ] || die "$(basename "$out"): no Mach-O files found"
}

# merge_zip <arm64 zip> <x86_64 zip> <output zip> app|plugin
merge_zip() {
    local name
    name="$(basename "$3" .zip)"
    ditto -x -k "$1" "$tmp/$name/arm64"
    ditto -x -k "$2" "$tmp/$name/x86_64"
    merge_tree "$tmp/$name/arm64" "$tmp/$name/x86_64" "$tmp/$name/universal"
    rm -f "$3"
    if [ "$4" == "app" ]; then
        sl_macos_zip "$tmp/$name/universal/StudioLink.app" "$3"
    else
        # plugin bundles at the top level of the zip (like dist/build.sh)
        (cd "$tmp/$name/universal" && ditto -c -k --sequesterRsrc . "$3")
    fi
    echo "universal: $3"
}

# Standalone (StudioLink.app)
#-----------------------------------------------------------------------------
shopt -s nullglob
zips_arm64=("$dir_arm64"/studio-link-standalone-*-arm64.zip)
zips_x86_64=("$dir_x86_64"/studio-link-standalone-*-x86_64.zip)
shopt -u nullglob

[ ${#zips_arm64[@]} -eq 1 ] ||
    die "expected one studio-link-standalone-<version>-arm64.zip in $dir_arm64"
[ ${#zips_x86_64[@]} -eq 1 ] ||
    die "expected one studio-link-standalone-<version>-x86_64.zip in $dir_x86_64"

name="$(basename "${zips_arm64[0]}" -arm64.zip)"
[ "$name" == "$(basename "${zips_x86_64[0]}" -x86_64.zip)" ] ||
    die "version differs: ${zips_arm64[0]} ${zips_x86_64[0]}"

merge_zip "${zips_arm64[0]}" "${zips_x86_64[0]}" "$out_dir/$name.zip" app

# DAW plugins (CLAP, VST3, AUv2)
#-----------------------------------------------------------------------------
for zip in studio-link-plugin.zip studio-link-plugin-onair.zip; do
    if [ ! -f "$dir_arm64/$zip" ] && [ ! -f "$dir_x86_64/$zip" ]; then
        echo "no $zip (SL_SKIP_PLUGINS?), skipped"
        continue
    fi
    [ -f "$dir_arm64/$zip" ] || die "$zip missing in $dir_arm64"
    [ -f "$dir_x86_64/$zip" ] || die "$zip missing in $dir_x86_64"
    merge_zip "$dir_arm64/$zip" "$dir_x86_64/$zip" "$out_dir/$zip" plugin
done
