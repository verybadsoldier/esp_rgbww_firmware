#!/bin/bash
set -e # exit with nonzero exit code if anything fails

if [ "$TRAVIS_PULL_REQUEST" == "true" ]; then
	echo "Pull request - skipping deploy!"
	exit 0;
fi

echo "New release - deploying - $TRAVIS_TAG"

FEED="<not set>"
if [ -z "$TRAVIS_TAG" ]; then
	# not a tag build so we will have proper branch value
	if [ "$TRAVIS_BRANCH" == "master" ]; then
		echo "Not a release - skipping deploy!"
		exit 0
	fi
	FEED="$TRAVIS_BRANCH"
else
	FEED="release"
	if [[ $TRAVIS_TAG == *"-"* ]]; then
		FEED="testing"
	fi
fi

# use feed "testing" if tag contains '-' (e.g. 4.0.1-rc1)

#COMITTER_NAME="Deployment Bot"
#COMMITER_EMAIL="<noemail>@<noemail.com>"

GH_PAGE_LINK="http://rgbww.dronezone.de/$FEED"
WEBAPP_VERSION="0.3.3"

GIT_DEPLOY_BRANCH="gh-pages"
GIT_DEPLOY_REPO="github.com/verybadsoldier/esp_rgbww_firmware.git"

ARTIFACTS_DIR="$TRAVIS_BUILD_DIR/out/Esp8266/release/firmware/"


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
{"rom":{"fw_version":"${FEED}","url":"${GH_PAGE_LINK}/rom0.bin"},"spiffs":{"webapp_version":"${WEBAPP_VERSION}","url":"${GH_PAGE_LINK}/spiff_rom.bin"}}
EOF

# committing...
git add .
git commit -m "Feed $FEED: Firmware v${TRAVIS_TAG} webapp v${WEBAPP_VERSION}"
git push