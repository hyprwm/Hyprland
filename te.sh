#!/bin/bash

build() {
  meson setup build \
    --wipe \
    --prefix /usr \
    --libexecdir lib \
    --buildtype debug \
    --wrap-mode nodownload \
    -D warning_level=2 \
    -D b_lto=true \
    -D b_pie=true \
    -D default_library=shared \
    -D xwayland=enabled \
    -D systemd=enabled

  meson compile -C build
}

build
