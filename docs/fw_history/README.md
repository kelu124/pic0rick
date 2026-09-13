# fw_history — reference firmware baselines

A **maintainer-curated** archive of known-good firmware `.uf2` binaries, kept as
regression baselines: a place to grab a firmware that is *known* to behave a
certain way (e.g. produce echoes) and A/B it against a current build.

## Naming

`fw<A.B.C>-<board>-<options>.uf2`, e.g. `fw0.1.1-RP2350-muxenabled.uf2`.
Include the firmware version, the board, and the notable build options.

## Rule — only the maintainer adds `.uf2` here

**Claude must never add, copy, build into, or commit `.uf2` files in this
folder.** These baselines are curated by the maintainer (Luc) only; they are
deliberate references, not build output. Claude's builds belong in
`firmware/dist/` (gitignored) and in GitHub releases.

Claude *may* edit this README, reference these baselines, and **flash** them to a
board for diagnostics — just not add new ones.

## Current baselines

- `fw0.1.1-RP2350-muxenabled.uf2` — last firmware before the DSP unification;
  the **stdio / old `adc.pio` pulser**, confirmed to produce strong pulse-echo
  signals (see `python/example_simple.ipynb`). Used as the reference when
  diagnosing the post-unification "no echo" regression (fixed in fw v0.1.11 —
  a MUX/pulser PIO1 state-machine conflict).

## Related

- `python/example_simple.ipynb` — minimal connect + capture that shows the echo
  peaks (variance in samples 2000–3000 ≫ 6000–7000) with this baseline.
- `docs/dsp_test_guide.md`, `firmware/CHANGELOG.md`.
