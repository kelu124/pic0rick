#!/bin/bash
# Build every firmware variant into firmware/dist/:
#   rp2040 {mux, nomux}                 (stdio, non-DSP)
#   rp2350 {mux, nomux}                 (stdio, non-DSP)
#   rp2350 {mux, nomux} + DSP           (raw-TinyUSB DSP build, needs CMSIS-DSP)
# The rp2040.uf2 / rp2350.uf2 at the firmware/ root are the default (mux,
# non-DSP) builds, kept for backwards compatibility.
set -e

VERSION=$(grep -E '^version:' version.yaml | sed -E 's/version:[[:space:]]*"?([0-9.]+)"?.*/\1/')
echo "Building pic0rick firmware v${VERSION}"
mkdir -p dist

# Shared FetchContent cache so the DSP builds clone CMSIS-DSP only once
# (and CI can cache this one directory).
DEPS_DIR="${PWD}/.deps"

# args: build-dir  pico-board  MUX(ON|OFF)  DSP(ON|OFF)  variant-label
build_one() {
    local dir=$1 board=$2 mux=$3 dsp=$4 label=$5
    echo "==> ${label} (board=${board}, MUX=${mux}, DSP=${dsp})"
    local extra=()
    if [ "$dsp" = "ON" ]; then
        extra+=("-DFETCHCONTENT_BASE_DIR=${DEPS_DIR}")
    fi
    cmake -B "$dir" -DPICO_BOARD="$board" -DMUX="$mux" -DDSP="$dsp" \
        "${extra[@]}" >/dev/null
    cmake --build "$dir" -j4
    cp "$dir/adc-pulse.uf2" "dist/pic0rick-v${VERSION}-${label}.uf2"
}

build_one build2040           pico  ON  OFF rp2040-mux
build_one build2040-nomux     pico  OFF OFF rp2040-nomux
build_one build2350           pico2 ON  OFF rp2350-mux
build_one build2350-nomux     pico2 OFF OFF rp2350-nomux
build_one build2350-dsp       pico2 ON  ON  rp2350-mux-dsp
build_one build2350-dsp-nomux pico2 OFF ON  rp2350-nomux-dsp

# Keep the historical default filenames (mux, non-DSP) at the firmware/ root.
cp "dist/pic0rick-v${VERSION}-rp2040-mux.uf2" rp2040.uf2
cp "dist/pic0rick-v${VERSION}-rp2350-mux.uf2" rp2350.uf2

echo "Done. Artifacts in firmware/dist/:"
ls -1 dist/
