#!/bin/bash -ex
#
# Local universal macOS build: builds arm64 and x86_64 (one after another,
# each with its own 3rdparty libs) and merges StudioLink.app into
# build/artifacts/macos_universal/studio-link-standalone-<version>.zip
#
# Works on Apple Silicon and Intel Macs. SL_SKIP_PLUGINS=1 skips the DAW
# plugins (they stay per architecture).
#
# CI builds the architectures in parallel jobs and merges the artifacts with
# dist/macos_universal.sh.
#

for target in macos_arm64 macos_x86_64; do
    BUILD_OS=macos BUILD_TARGET=$target dist/build.sh
done

source dist/lib/versions.sh
source dist/lib/functions.sh
sl_prepare_version

dist/macos_universal.sh \
    build/artifacts/macos_arm64/studio-link-standalone-$version_t-arm64.zip \
    build/artifacts/macos_x86_64/studio-link-standalone-$version_t-x86_64.zip \
    build/artifacts/macos_universal
