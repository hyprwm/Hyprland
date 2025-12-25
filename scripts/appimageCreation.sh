#!/bin/bash

set -e

SKIP_DEPS="${SKIP_DEPS:-0}"

ARCH=$(uname -m)
ARCHDASH=$(echo "$ARCH"|tr '_' '-')
APPIMAGETOOL="https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-${ARCH}.AppImage"

curl -Ls "$APPIMAGETOOL" > appimagetool
if [ $? -ne 0 ];then
	echo "failed to retrieve appimagetool.  bailing out."
	exit 1
fi
chmod -f +x appimagetool

TESTPROGRAM='
#include "zip.h"

int main()
{
  zip_compression_method_supported(ZIP_CM_DEFAULT,1);
  return 0;
}
'

WM_SCRIPT='#!/usr/bin/env bash
export LD_LIBRARY_PATH="${APPIMAGE_LIBRARY_PATH}:${APPDIR}/usr/lib:${LD_LIBRARY_PATH}"

#if we have symlinked to this program it may be attempting to call a different hyprland executable
CALLER=$(basename "$ARGV0")
if [ ! -e "${APPDIR}/usr/bin/${CALLER}" ];then
	#by default call hyprland
	CALLER=hyprland
fi
EXEC="${APPDIR}/usr/bin/${CALLER}"

"${APPDIR}/usr/bin/test" >/dev/null 2>&1 |:
FAIL=${PIPESTATUS[0]}
if [ $FAIL -eq 0 ];then
  echo "executing with native libc"
  $EXEC $*
else
  echo "executing with appimage libc"
  export LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:${APPDIR}/usr/lib-override"
  "${APPDIR}/usr/lib-override/ld-linux-'$ARCHDASH'.so.2" $EXEC $*
fi
'

DIR="$(pwd)"

if [ -d AppDir ];then
	rm -rf AppDir
fi
mkdir -p "$DIR/AppDir/usr/bin" || exit 1
mkdir -p "$DIR/AppDir/usr/lib" || exit 1
mkdir -p "$DIR/AppDir/usr/lib-override" || exit 1

#build test program
echo "$TESTPROGRAM" > "$DIR/test.c" || exit 2
gcc "$DIR/test.c" -o "$DIR/AppDir/usr/bin/test" -lzip || exit 2
rm -f "$DIR/AppDir/test.c" || exit 2

cp -f /usr/local/bin/* "$DIR/AppDir/usr/bin/." || exit 4
rm -f "$DIR/AppDir/AppRun"
echo "$WM_SCRIPT" > "$DIR/AppDir/AppRun" || exit 4
chmod +x "$DIR/AppDir/AppRun" || exit 4
cp "$DIR/example/hyprland.desktop" "$DIR/AppDir/hyprland.desktop" || exit 4
cp "$DIR/assets/hyprland.png" "$DIR/AppDir/hyprland.png" || exit 4
mkdir -p "$DIR/AppDir/usr/share/metainfo"
ldd "$DIR/AppDir/usr/bin/Hyprland" | \
	grep --color=never -v libGL| \
	grep --color=never -v libdrm.so | \
	grep --color=never -v libgbm.so | \
	awk '{print $3}'| \
	xargs -I% cp -Lf "%" "$DIR/AppDir/usr/lib/." || exit 5
strip -s "$DIR/AppDir/usr/lib/"* || exit 5
strip -s "$DIR/AppDir/usr/bin/"* || exit 5
mv -f "$DIR/AppDir/usr/lib/libc.so.6" "$DIR/AppDir/usr/lib-override/."
mv -f "$DIR/AppDir/usr/lib/libm.so.6" "$DIR/AppDir/usr/lib-override/."
cp -f "$(ldconfig -Np|grep --color=never libpthread.so.0$|grep --color=never $ARCHDASH|awk '{print $NF}')" "$DIR/AppDir/usr/lib-override/."
cp -Lf "/lib64/ld-linux-${ARCHDASH}.so.2" "$DIR/AppDir/usr/lib-override/." || exit 6

cd "$DIR" || exit 5
rm -rf ./output
mkdir ./output
APPIMAGENAME="hyprland-${ARCH}.AppImage"

./appimagetool AppDir ./output/$APPIMAGENAME || exit 7

#create script for symlinks, can't include these in the output/archive as github will zip this up without symlink support
for bin in "${DIR}/AppDir/usr/bin/"[Hh]ypr*;do
	echo "ln -sf $APPIMAGENAME $(basename $bin)" >> ./output/hyprland-create-symlinks.sh
done
chmod +x ./output/*
