#!/usr/bin/env -S nix shell nixpkgs#gawk nixpkgs#git nixpkgs#gnused nixpkgs#ripgrep -c bash

# get wlroots revision from submodule
SUB_REV=$(git submodule status | rg wlroots | awk '{ print substr($1,2) }')
# and from lockfile
CRT_REV=$(rg rev flake.nix | awk '{ print substr($3, 2, 40) }')

if [ "$SUB_REV" != "$CRT_REV" ]; then
  echo "Updating wlroots..."
  # update wlroots to submodule revision
  sed -Ei "s/\w{40}/$SUB_REV/g" flake.nix
  nix flake lock

  echo "wlroots: $CRT_REV -> $SUB_REV"
else
  echo "wlroots is up to date!"
fi
