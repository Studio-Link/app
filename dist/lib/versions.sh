vmajor=20; vminor=04; vpatch=5
vbuild="$(git rev-list HEAD --count).$(git rev-parse --short HEAD)"
release="alpha-quick-${vbuild}"
#release="beta"
baresip="20.03.1"
baresip_lib="0.6.5"
re="20.03.0"
rem="0.6.0"
sl3rdparty="v20.04.4"
overlay="v19.09.0"
juce="5.4.4"
github_org="https://github.com/Studio-Link"
vstsdk="vstsdk367_03_03_2017_build_352"
debug="RELEASE=1"
if [ -z "${TRAVIS_BRANCH}" ]; then
TRAVIS_BRANCH=$(git rev-parse --abbrev-ref HEAD)
fi
