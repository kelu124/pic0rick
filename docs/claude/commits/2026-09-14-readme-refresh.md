# 2026-09-14 — README refresh (post-unification), on main

Docs only. Committed directly to `main` (post-merge) at the user's request.

## What
- **Quick start** rewritten for the DSP build: `set_gain` + `configure_pulse` +
  `arm_pulser` + `read_raw()` (the old `dac`/`pulse_adc_trigger`/`read` were
  stdio-only and don't transmit on the DSP build the board runs). Notes the stdio
  alternative; `from_probe` example kept (it drives the DSP build internally).
- **Serial command reference** relabelled as the stdio/non-DSP build; fixed the
  mux rows (`write mux <hex>`, `set/clear mux` take no arg); pointer to the DSP
  build's command set.
- **DSP build** section: added `dac write` / `pulse config` / `pulser` commands
  and the `Pic0rick` helper list; links to the test guide + notebooks.
- **Signal chain**: pulser pins corrected to GPIO11/12 (P+/P-) + GPIO16/17
  (PDAMP/OE), MD1213.
- Added a **Repository layout** table (firmware/, python/, hardware/, docs/,
  .github/workflows/).

## Verification
Checked every README Python reference against the code (`.venv`): all
`Pic0rick` methods exist, `from_probe` signature matches, `pic0rick.dsp` helpers
present.
