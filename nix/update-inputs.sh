#!/usr/bin/env -S nix shell nixpkgs#gawk nixpkgs#git nixpkgs#moreutils -c bash

# get wlroots revision from submodule
rev=$(git submodule status | awk '{ print substr($1,2)}')

# update nixpkgs to latest version
nix flake lock --update-input nixpkgs

# update wlroots to submodule revision
nix flake lock --override-input wlroots "gitlab:wlroots/wlroots/$rev?host=gitlab.freedesktop.org"

# remove "dirty" mark from lockfile
jq < flake.lock 'del(.nodes.wlroots.original.rev)' | sponge flake.lock
