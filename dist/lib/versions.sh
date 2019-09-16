vmajor=19; vminor=9; vpatch=0
vbuild="$(git rev-list HEAD --count).$(git rev-parse --short HEAD)"
#release="alpha-${vbuild}"
release="beta"
baresip="0.6.3"
re="0.6.1"
rem="0.6.0"
opus="1.3.1"
openssl="1.1.1c"
openssl_sha256="f6fb3079ad15076154eda9413fed42877d668e7069d9b87396d0804fdb3f4c90"
rtaudio="master"
juce="5.4.4"
flac="1.3.2"
github_org="https://github.com/Studio-Link"
vstsdk="vstsdk367_03_03_2017_build_352"
debug="RELEASE=1"
if [ -z "${TRAVIS_BRANCH+x}" ]; then
TRAVIS_BRANCH=$(git rev-parse --abbrev-ref HEAD)
fi
