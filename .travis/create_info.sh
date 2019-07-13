#!/bin/sh

TIMESTAMP=$(date "+%Y-%m-%d %H:%M:%S")
echo "Build Timestamp: $TIMESTAMP\nCommit: $TRAVIS_COMMIT\nBranch: $TRAVIS_BRANCH" > $1
