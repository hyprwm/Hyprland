#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FLAMEGRAPH_DIR="$SCRIPT_DIR/flamegraph"

if [ ! -d "$FLAMEGRAPH_DIR" ]; then
    echo "Cloning FlameGraph tools..."
    git clone https://github.com/brendangregg/FlameGraph "$FLAMEGRAPH_DIR"
fi

if [ ! -f perf.data ]; then
    echo "No perf.data found in current directory."
    echo "Run Hyprland under perf first:"
    echo "  perf record -F 99 -g -- ./build/Hyprland"
    exit 1
fi

echo "Generating flame graph..."
perf script | "$FLAMEGRAPH_DIR/stackcollapse-perf.pl" | "$FLAMEGRAPH_DIR/flamegraph.pl" > flame.svg
xdg-open flame.svg
