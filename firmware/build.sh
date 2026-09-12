#!/bin/bash
# Build every firmware variant: {rp2040, rp2350} x {mux, nomux}.
# Output .uf2 files land in firmware/dist/ (gitignored). The rp2040.uf2 /
# rp2350.uf2 copies at the repo firmware/ root are the default (mux) builds.
set -e

VERSION=$(grep -E '^version:' version.yaml | sed -E 's/version:[[:space:]]*"?([0-9.]+)"?.*/\1/')
echo "Building pic0rick firmware v${VERSION}"
mkdir -p dist

# args: build-dir  pico-board  MUX(ON|OFF)  variant-label
build_one() {
    local dir=$1 board=$2 mux=$3 label=$4
    echo "==> ${label} (board=${board}, MUX=${mux})"
    cmake -B "$dir" -DPICO_BOARD="$board" -DMUX="$mux" >/dev/null
    cmake --build "$dir" -j4
    cp "$dir/adc-pulse.uf2" "dist/pic0rick-v${VERSION}-${label}.uf2"
}

build_one build2040       pico  ON  rp2040-mux
build_one build2040-nomux pico  OFF rp2040-nomux
build_one build2350       pico2 ON  rp2350-mux
build_one build2350-nomux pico2 OFF rp2350-nomux

# Keep the historical default filenames (mux build) at the firmware/ root.
cp dist/pic0rick-v${VERSION}-rp2040-mux.uf2 rp2040.uf2
cp dist/pic0rick-v${VERSION}-rp2350-mux.uf2 rp2350.uf2

echo "Done. Artifacts in firmware/dist/:"
ls -1 dist/
