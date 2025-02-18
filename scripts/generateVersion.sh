#!/bin/sh

# if the git directory doesn't exist, don't gather data to avoid overwriting, unless
# the version file is missing altogether (otherwise compiling will fail)
if [ ! -d ./.git ]; then
    if [ -f ./src/version.h ]; then
        exit 0
    fi
fi

cp -fr ./src/version.h.in ./src/version.h

HASH=${HASH-$(git rev-parse HEAD)}
BRANCH=${BRANCH-$(git branch --show-current)}
MESSAGE=${MESSAGE-$(git show | head -n 5 | tail -n 1 | sed -e 's/#//g' -e 's/\"//g')}
DATE=${DATE-$(git show --no-patch --format=%cd --date=local)}
DIRTY=${DIRTY-$(git diff-index --quiet HEAD -- || echo dirty)}
TAG=${TAG-$(git describe --tags)}
COMMITS=${COMMITS-$(git rev-list --count HEAD)}

sed -i -e "s#@HASH@#${HASH}#" ./src/version.h
sed -i -e "s#@BRANCH@#${BRANCH}#" ./src/version.h
sed -i -e "s#@MESSAGE@#${MESSAGE}#" ./src/version.h
sed -i -e "s#@DATE@#${DATE}#" ./src/version.h
sed -i -e "s#@DIRTY@#${DIRTY}#" ./src/version.h
sed -i -e "s#@TAG@#${TAG}#" ./src/version.h
sed -i -e "s#@COMMITS@#${COMMITS}#" ./src/version.h
