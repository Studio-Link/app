vmajor=26; vminor=11; vpatch=0
vbuild="$(git rev-parse --short HEAD)"
release="beta-${vbuild}"
#release="stable"
baresip="4.13.0"
re="4.13.0"
# 3rdparty release with PortAudio (instead of libsoundio), see
# https://github.com/Studio-Link/3rdparty
sl3rdparty="v26.11.0"
github_org="https://github.com/Studio-Link"
# DAW plugins: clap-wrapper (pins the CLAP, VST3 and AudioUnit SDKs)
clap_wrapper="v0.17.0"
# Embedded CA certificates, https://curl.se/ca/cacert-<date>.pem
cacert="2026-11-05"
cacert_sha256="427c064216761a1e72a8b30f0d954d08cb6e4435a82091fb865afd20c8497285"
# Debug build: BUILD_TYPE=Debug dist/build.sh
build_type="${BUILD_TYPE:-Release}"
