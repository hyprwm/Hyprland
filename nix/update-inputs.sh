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

  # update inputs to latest versions
  nix flake update
else
  echo "nixpkgs is up to date!"
fi
