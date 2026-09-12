# 2026-09-13 — Firmware version command, optional MUX (-DMUX), CI release

Firmware **v0.1.0** (first versioned release).

## What
1. **Versioning** — added `firmware/version.yaml` (`version: "A.B.C"` +
   `changes`) as the single source of truth. `CMakeLists.txt` parses it and
   `configure_file`s `version.h.in` → generated `version.h` (`FW_VERSION`,
   `FW_CHANGES`). New `version` CLI command prints version + changes + mux state.
2. **Optional MUX** — new CMake `option(MUX ... ON)`. When `-DMUX=OFF`,
   `max/max14866.c` is excluded and `MUX` is undefined, so `main.c` omits the
   `write/set/clear mux` commands and `max14866_init()`.
3. **DAC split** — moved the MCP4812 gain DAC out of `max/max14866.c` into new
   `max/dac.c` + `max/dac.h`. The DAC is always built (gain control is not part
   of the mux module). Both reuse the `max14866.pio` SPI-shifter program, which
   is always generated.
4. **build.sh** — now builds 4 variants (rp2040/rp2350 × mux/nomux) into
   `firmware/dist/` and refreshes the historical `rp2040.uf2` / `rp2350.uf2`
   (mux builds).
5. **CI** — `.github/workflows/firmware.yml`: build all 4 on push/PR to main,
   upload artifacts; on **push to main** publish a release `fw-v<version>` with
   the 4 uf2 attached and `changes` as the body. Skips if the tag exists; never
   releases on other branches.

## Why
User wants: a version command sourced from a yaml (with a changelog line usable
by the release workflow); the MAX14866 plug-in board selectable at compile time
producing separate artifacts; and automatic releases from main.

## Verification
Built all 4 variants locally (pico-sdk 2.2.0, arm-none-eabi-gcc). Confirmed:
- `version.h` embeds `0.1.0` + changes text.
- mux build compiles `max14866.c.o` + `dac.c.o`; nomux build compiles only
  `dac.c.o` (max14866 correctly excluded).

## Notes / caveats
- Release is skipped when `fw-v<version>` already exists (can't reuse a tag), so
  a main push that doesn't bump the firmware version produces artifacts but no
  new release. This is intentional (see TODO if the maintainer wants per-commit
  releases regardless).
- CI pins pico-sdk to 2.2.0 (matches CMakeLists sdkVersion).

## Rule added
Firmware patch (C) is bumped by Claude on every change to `firmware/` files;
maintainer owns major/minor. Recorded in `docs/claude/README.md` and memory.
