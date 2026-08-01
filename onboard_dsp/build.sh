#!/bin/bash
set -e

# rp2350
cmake -B build2350 -DPICO_BOARD=pico2
cmake --build build2350 -j4
cp build2350/pic0rick-envelope.uf2 pic0rick-envelope.uf2
