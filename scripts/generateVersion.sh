#!/bin/sh
cp -fr ./src/version.h.in ./src/version.h

HASH=$(git rev-parse HEAD)
BRANCH=$(git rev-parse --abbrev-ref HEAD)
MESSAGE=$(git show ${GIT_COMMIT_HASH} | head -n 5 | tail -n 1 | sed -e 's/#//g' -e 's/\"//g')
DATE=$(git show ${GIT_COMMIT_HASH} --no-patch --format=%cd --date=local)
DIRTY=$(git diff-index --quiet HEAD -- || echo dirty)
TAG=$(git describe --tags)

sed -i -e "s#@HASH@#${HASH}#" ./src/version.h
sed -i -e "s#@BRANCH@#${BRANCH}#" ./src/version.h
sed -i -e "s#@MESSAGE@#${MESSAGE}#" ./src/version.h
sed -i -e "s#@DATE@#${DATE}#" ./src/version.h
sed -i -e "s#@DIRTY@#${DIRTY}#" ./src/version.h
sed -i -e "s#@TAG@#${TAG}#" ./src/version.h
