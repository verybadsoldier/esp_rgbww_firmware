#!/bin/bash
set -e # exit with nonzero exit code if anything fails

if [ -z "$TRAVIS_TAG" ] || [ "$TRAVIS_PULL_REQUEST" == "true" ]; then
	echo "Not a release - skipping deploy!"
	#exit 0;
	$TRAVIS_TAG="9000.0"
fi

echo "New release - deploying - $TRAVIS_TAG"

# use channel "testing" if tag contains '-' (e.g. 4.0.1-rc1)
CHANNEL="release"
if [[ $TRAVIS_TAG == *"-"* ]]; then
	CHANNEL="testing"
fi

#COMITTER_NAME="Deployment Bot"
#COMMITER_EMAIL="<noemail>@<noemail.com>"

GH_PAGE_LINK="http://rgbww.dronezone.de/$CHANNEL/"
WEBAPP_VERSION="0.3.3"

GIT_DEPLOY_BRANCH="gh_pages_test"
GIT_DEPLOY_REPO="github.com/verybadsoldier/esp_rgbww_firmware.git"

echo "Using channel: $CHANNEL"

git clone https://${GITHUB_TOKEN}@$GIT_DEPLOY_REPO --recursive --branch $GIT_DEPLOY_BRANCH --single-branch deploy_release

ls deploy_release

# prepare folder
mkdir -p $TRAVIS_BUILD_DIR/deploy_release/$CHANNEL
cd $TRAVIS_BUILD_DIR/deploy_release/$CHANNEL

ls

ls $ARTIFACTS_DIR

# copy firmware files
cp $ARTIFACTS_DIR/* .

# create version information
cat <<EOF > version.json
{"rom":{"fw_version":"${$TRAVIS_TAG}","url":"${GH_PAGE_LINK}/rom0.bin"},"spiffs":{"webapp_version":"${WEBAPP_VERSION}","url":"${GH_PAGE_LINK}/spiff_rom.bin"}}
EOF

cat version.json

mv $TRAVIS_BUILD_DIR/release $TRAVIS_BUILD_DIR/_release/
mv $TRAVIS_BUILD_DIR/esp_rgbww_webinterface/dist/esp_rgbww_webinterface.zip $TRAVIS_BUILD_DIR/_release/release/esp_rgbww_webinterface.zip

git add .
git commit -m "Release Firmware v${FW_VERSION} webapp v${WEBAPP_VERSION}"
git push
