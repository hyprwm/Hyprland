#!/usr/bin/env -S nix shell nixpkgs#gawk nixpkgs#git nixpkgs#moreutils nixpkgs#jq nixpkgs#ripgrep -c bash

set -ex

# get wlroots revision from submodule
SUB_REV=$(git submodule status | rg wlroots | awk '{ print substr($1,2)}')
# and from lockfile
CRT_REV=$(jq < flake.lock '.nodes.wlroots.locked.rev' -r)

if [ $SUB_REV != $CRT_REV ]; then
  # update nixpkgs to latest version
  nix flake lock --update-input nixpkgs

  # update wlroots to submodule revision
  nix flake lock --override-input wlroots "gitlab:wlroots/wlroots/$SUB_REV?host=gitlab.freedesktop.org"

  # remove "dirty" mark from lockfile
  jq < flake.lock 'del(.nodes.wlroots.original.rev)' | sponge flake.lock
else
  echo "wlroots is up to date!"
fi
