#!/usr/bin/env bash
set -euo pipefail

dir="${1:-.github/DISCUSSION_TEMPLATE}"

bugs=(
  bugs-cmake-meson-nix.yml
  bugs-config.yml
  bugs-crashes.yml
  bugs-drm.yml
  bugs-input.yml
  bugs-ipc-plugins.yml
  bugs-layout.yml
  bugs-other.yml
  bugs-performance.yml
  bugs-renderer.yml
  bugs-window-management.yml
  bugs-xwayland.yml
)

features=(
  feature-requests-config.yml
  feature-requests-layouts.yml
  feature-requests-other.yml
  feature-requests-protocols-integrations.yml
  feature-requests-renderer.yml
  feature-requests-window-management.yml
)

generate_templates() {
  local source="$1"
  shift

  if [[ ! -f "$dir/$source" ]]; then
    printf 'error: source template not found: %s\n' "$dir/$source" >&2
    return 1
  fi

  local filename
  for filename in "$@"; do
    cp -- "$dir/$source" "$dir/$filename"
    printf 'generated %s from %s\n' "$filename" "$source"
  done
}

generate_templates bugs.yml "${bugs[@]}"
generate_templates features.yml "${features[@]}"