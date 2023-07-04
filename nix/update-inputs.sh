#!/usr/bin/env -S nix shell nixpkgs#jq -c bash

# Update inputs when the Mesa version is outdated. We don't want
# incompatibilities between the user's system and Hyprland.

# get the current Nixpkgs revision
REV=$(jq <flake.lock '.nodes.nixpkgs.locked.rev' -r)
# check versions for current and remote nixpkgs' mesa
CRT_VER=$(nix eval --raw github:nixos/nixpkgs/"$REV"#mesa.version)
NEW_VER=$(nix eval --raw github:nixos/nixpkgs/nixos-unstable#mesa.version)

if [ "$CRT_VER" != "$NEW_VER" ]; then
  echo "Updating Mesa $CRT_VER -> $NEW_VER and flake inputs"

  # keep wlroots rev, as we don't want to update it
  WLR_REV=$(nix flake metadata --json | jq -r '.locks.nodes.wlroots.locked.rev')

  # update inputs to latest versions
  nix flake update

  # hold back wlroots (nix/update-wlroots.nix handles updating that)
  nix flake lock --override-input wlroots "gitlab:wlroots/wlroots/$WLR_REV?host=gitlab.freedesktop.org"
else
  echo "nixpkgs is up to date!"
fi
