#!/bin/bash
set -e # exit with nonzero exit code if anything fails

if [ -z "$TRAVIS_TAG" ] || [ "$TRAVIS_PULL_REQUEST" == "true" ]; then
	echo "Not a release - skipping deploy!"
	exit 0;
fi

echo "New release - deploying - $TRAVIS_TAG"

# use feed "testing" if tag contains '-' (e.g. 4.0.1-rc1)
if [[ $TRAVIS_TAG == *"-alpha"* ]]; then
    FEED="unstable"
elif [[ $TRAVIS_TAG == *"-"* ]]; then
    FEED="testing"
else
    FEED="release"
fi

GH_PAGE_LINK="http://rgbww.dronezone.de/$FEED/"
WEBAPP_VERSION="0.3.3"

GIT_DEPLOY_BRANCH="gh-pages"
GIT_DEPLOY_REPO="github.com/verybadsoldier/esp_rgbww_firmware.git"

echo "Using feed: $FEED"

cd $TRAVIS_BUILD_DIR
git clone https://${GITHUB_TOKEN}@$GIT_DEPLOY_REPO --recursive --branch $GIT_DEPLOY_BRANCH --single-branch deploy_release

# prepare folder
mkdir -p $TRAVIS_BUILD_DIR/deploy_release/$FEED
cd $TRAVIS_BUILD_DIR/deploy_release/$FEED

# copy firmware files
cp $ARTIFACTS_DIR/* .

# create version information
cat <<EOF > version.json
{"rom":{"fw_version":"${TRAVIS_TAG}","url":"${GH_PAGE_LINK}/rom0.bin"},"spiffs":{"webapp_version":"${WEBAPP_VERSION}","url":"${GH_PAGE_LINK}/spiff_rom.bin"}}
EOF

# committing
git add .
git commit -m "Feed $FEED: Firmware v${TRAVIS_TAG} webapp v${WEBAPP_VERSION}"
git push
