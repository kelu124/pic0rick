# TODO — pic0rick

Open, actionable items for Claude Code and the maintainer. Completed items move
to `DONE.md`. Every commit that touches this repo also gets a log entry in
`docs/claude/commits/`.

Legend: **[claude]** doable by Claude from the code · **[maintainer]** needs a
hardware fact or product decision only Luc can supply · **[mixed]** Claude can
implement once a maintainer decision/measurement is provided.

Sourced from the former `QUESTIONS.md` (documentation review, 2026-09-12) plus
discoveries logged since.

---

## Firmware

- [ ] **[maintainer]** Confirm the DMA-timeout race fix is wanted: insert
  `sleep_us(1)` after `pio_sm_put_blocking(pio_adc, sm, SAMPLE_COUNT)` in
  `pulse_adc_trigger` (`firmware/adc/adc.c:131`) so the ADC SM reaches its
  `wait irq 0` state before the pulse fires. *Note: the `TODO.md` that the old
  QUESTIONS.md cited does not exist in the repo — this is the only surviving
  record of that fix.* (was Q1)
- [ ] **[maintainer]** RP2350 build status: is `build2350/` tested and confirmed
  working? Document any deltas from RP2040 (clock dividers, PIO programs, DMA
  channels). (was Q2)
- [ ] **[maintainer]** Document hardware-safe upper limits for `pon` / `poff` /
  `damp` (ns). Firmware accepts any `int`; the pulser board has real limits
  (inductor saturation, HV cap discharge). (was Q3)
- [ ] **[mixed]** `SAMPLE_COUNT` is hardcoded to 8000 in `firmware/adc/adc.h`.
  Decide whether to expose it as a serial command (needs a runtime-sized DMA
  buffer) or keep it a compile-time constant; document the choice. (was Q4)

## Hardware

- [ ] **[maintainer]** Characterise HV rail voltage spread across production
  boards (IAS0105D24 spec ±24V; no on-board monitoring). (was Q5)
- [ ] **[maintainer]** Document the `OE` signal on J5 pin 3: active-high/low, and
  whether it is RP2040-driven or tied static. (was Q6)
- [ ] **[maintainer]** Reconcile production PCB vs. `main`-branch KiCad. Production
  appears to be commit `bab83371f452f650acd79c82d3ccac702bd4d4ba`; either note
  the production tag in the README or update the main-branch files. (was Q7)
- [ ] **[maintainer]** Locate/add the MUX board KiCad files. `write/set/clear mux`
  (MAX14866) are in firmware and the README lists a MUX design entry, but there
  is no `hardware/mux/` directory. (was Q8)

## Python library

- [ ] **[mixed]** Publish a `dac_to_db(n)` helper and label plots in dB. Needs
  the board VREF for the MCP4812 (AD8331 VGAIN: 0V→7.5dB, 1V→55.5dB). (was Q9)
- [ ] **[maintainer]** Verify `Fech = 60e6` (`device.py:64`) against a frequency
  counter or known-thickness calibration; add a trim/calibration path if it
  differs. (was Q10)
- [ ] **[maintainer]** Decide `requirements.txt` scope: replace the 122-package
  `pip freeze` with `requirements-minimal.txt`, or keep both (dev freeze + minimal
  runtime). (was Q11)

## Documentation

- [ ] **[maintainer]** Write step-by-step assembly instructions for the 3-board
  stack (main + pulser + HV), with reference-designator callouts on the
  `docs/images/` photos. (was Q12)
- [ ] **[maintainer]** Clarify status of the `VGA` proof-of-concept branch — flag
  as experimental/archived in the README if not maintained. (was Q13)
- [ ] **[maintainer]** Document tested transducer frequencies/types and whether
  immersion probes are supported (any hardware changes needed). (was Q14)

## Discoveries (logged by Claude)

- [ ] **[mixed]** MUX not wired into the high-level acquisition flow.
  `device.py` now exposes `write_mux` / `set_mux` / `clear_mux`, but
  `ndt_acquisition.py` (`UltrasonicAcquisition`) never selects a mux channel.
  For multi-element / array probing, integrate mux selection into the
  acquisition sequence. Needs a decision on the intended channel-map semantics
  (found 2026-09-12 during firmware↔Python parity check).
- [ ] **[maintainer]** Document the MAX14866 mux bit-map: which bits in the
  16-bit `write mux` word map to which physical switch/channel. Needed before
  `write_mux` can be used meaningfully from Python (found 2026-09-12).
- [ ] **[maintainer]** Release cadence decision: CI cuts a release only when
  `fw-v<version>` doesn't already exist, so a main push that doesn't bump the
  firmware patch yields artifacts but no new release. Confirm this is desired, or
  ask for per-commit releases (would need a unique tag e.g. `+<short-sha>`)
  (found 2026-09-13).
- [ ] **[maintainer]** First run of `.github/workflows/firmware.yml` needs
  `Settings → Actions → Workflow permissions = Read and write` (or the release
  step can't create tags/releases despite `permissions: contents: write`)
  (found 2026-09-13).
- [ ] **[claude]** The committed `firmware/rp2040.uf2` / `rp2350.uf2` are the
  mux builds refreshed at v0.1.0. Decide whether to keep binaries in git at all
  now that CI publishes releases, or drop them and point users to releases
  (found 2026-09-13).
