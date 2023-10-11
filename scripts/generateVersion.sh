#!/bin/sh
cp -fr ./src/version.h.in ./src/version.h

HASH=$(git rev-parse HEAD)
BRANCH=$(git rev-parse --abbrev-ref HEAD)
MESSAGE=$(git show ${GIT_COMMIT_HASH} | head -n 5 | tail -n 1 | sed -e 's/#//g' -e 's/\"//g')
DIRTY=$(git diff-index --quiet HEAD -- || echo \"dirty\")
TAG=$(git describe --tags)

awk -i inplace "{sub(/@HASH@/,\"${HASH}\")}1" ./src/version.h
awk -i inplace "{sub(/@BRANCH@/,\"${BRANCH}\")}1" ./src/version.h
awk -i inplace "{sub(/@MESSAGE@/,\"${MESSAGE}\")}1" ./src/version.h
awk -i inplace "{sub(/@DIRTY@/,\"${DIRTY}\")}1" ./src/version.h
awk -i inplace "{sub(/@TAG@/,\"${TAG}\")}1" ./src/version.h