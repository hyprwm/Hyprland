#!/usr/bin/env -S nix shell nixpkgs#jq -c bash

# Update inputs when the Mesa or QT version is outdated. We don't want
# incompatibilities between the user's system and Hyprland.

# get the current Nixpkgs revision
REV=$(jq <flake.lock '.nodes.nixpkgs.locked.rev' -r)

get_ver() {
  nix eval --raw "github:nixos/nixpkgs/$1#$2"
}

# check versions for current and remote nixpkgs'
MESA_OLD=$(get_ver "$REV" mesa.version)
MESA_NEW=$(get_ver nixos-unstable mesa.version)
QT_OLD=$(get_ver "$REV" kdePackages.qtbase.version)
QT_NEW=$(get_ver nixos-unstable kdePackages.qtbase.version)

if [ "$MESA_OLD" != "$MESA_NEW" ] || [ "$QT_OLD" != "$QT_NEW" ]; then
  echo "Updating flake inputs..."
  echo "Mesa: $MESA_OLD -> $MESA_NEW"
  echo "Qt:   $QT_OLD -> $QT_NEW"

  # update inputs to latest versions
  nix flake update
else
  echo "nixpkgs is up to date!"
fi
