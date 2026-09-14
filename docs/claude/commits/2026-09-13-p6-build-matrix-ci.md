# 2026-09-13 — P6: 6-variant build matrix + CI

Firmware **v0.1.9**. Branch `feature/unify-firmware-dsp`.

## What
- `build.sh` now builds **six** variants into `firmware/dist/`:
  `rp2040-{mux,nomux}`, `rp2350-{mux,nomux}` (stdio/non-DSP), and
  `rp2350-{mux,nomux}-dsp` (raw-TinyUSB DSP). Still copies the two mux non-DSP
  builds to `rp2040.uf2` / `rp2350.uf2`.
- DSP builds share one CMSIS-DSP clone via `-DFETCHCONTENT_BASE_DIR=firmware/.deps`
  (gitignored) so the second DSP build reuses the first's checkout.
- `.gitignore`: added `build2350-dsp*`, `.deps/`.
- CI (`.github/workflows/firmware.yml`): added an `actions/cache` step for
  `firmware/.deps` (key `cmsis-dsp-v1.17.0`). The build step already runs
  `./build.sh`, and the artifact upload + release `files: firmware/dist/*.uf2`
  automatically pick up all 6.

## Why
Expose every build combination as a downloadable artifact / release asset, and
keep CI (and repeated local builds) fast by not re-cloning CMSIS-DSP.

## Verification
`./build.sh` builds all 6 uf2 locally (first DSP build clones CMSIS-DSP into
`.deps`, second reuses it).
