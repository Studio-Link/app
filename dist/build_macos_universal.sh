#!/bin/bash -ex
#
# Local universal macOS build: builds arm64 and x86_64 (one after another,
# each with its own 3rdparty libs) and merges StudioLink.app and the DAW
# plugins into build/artifacts/macos_universal/
#   studio-link-standalone-<version>.zip
#   studio-link-plugin.zip, studio-link-plugin-onair.zip
#
# Works on Apple Silicon and Intel Macs. SL_SKIP_PLUGINS=1 skips the DAW
# plugins.
#
# CI builds the architectures in parallel jobs and merges the artifacts with
# dist/macos_universal.sh.
#

for target in macos_arm64 macos_x86_64; do
    BUILD_OS=macos BUILD_TARGET=$target dist/build.sh
done

rm -rf build/artifacts/macos_universal
dist/macos_universal.sh \
    build/artifacts/macos_arm64 \
    build/artifacts/macos_x86_64 \
    build/artifacts/macos_universal
