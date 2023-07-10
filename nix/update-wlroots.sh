#!/usr/bin/env -S nix shell nixpkgs#gawk nixpkgs#git nixpkgs#gnused nixpkgs#jq nixpkgs#ripgrep -c bash

# get wlroots revision from submodule
SUB_REV=$(git submodule status | rg wlroots | awk '{ print substr($1,2)}')
# and from lockfile
CRT_REV=$(jq <flake.lock '.nodes.wlroots.locked.rev' -r)

if [ "$SUB_REV" != "$CRT_REV" ]; then
  echo "Updating wlroots..."
  # update wlroots to submodule revision
  nix flake lock --override-input wlroots "gitlab:wlroots/wlroots/$SUB_REV?host=gitlab.freedesktop.org"

  # fix revision in wlroots.wrap
  sed -Ei "s/[a-z0-9]{40}/$SUB_REV/g" subprojects/wlroots.wrap
else
  echo "wlroots is up to date!"
fi
