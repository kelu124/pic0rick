# 2026-09-13 — Pulser echo regression fix

Firmware **v0.1.10**. Branch `feature/unify-firmware-dsp`.

## Symptom
User: echoes worked on stdio **v0.1.0** but not after the unification (piezo
connected, no echo). Suspected the OE pin.

## Root cause
Not OE. OE polarity is correct (active-high; the pulser board AND-gates the drive
with OE via a `TC74VHC08`, and the firmware drives OE high through the transmit).
The regression was the **pulse waveform structure**, introduced in P4b when the
stdio build adopted the shared `pulser.pio`:
- v0.1.0 (`adc.pio`): bipolar pulse `P+`→`P-` back-to-back, **then** PDAMP damp.
- shared pulser: negative → **PDAMP damp** → positive (damp *between* the
  polarities). With the default 6 µs damp this splits the bipolar excitation into
  two weak, far-apart spikes → poor transmit, no echo.

## Fix
Reordered `hw/acquisition.c` `queue_pulse` so the two drive polarities fire
back-to-back and the PDAMP damp phase comes **last** (matching v0.1.0 and
standard pulser practice). No PIO change; affects both the stdio and DSP builds
(they share `firmware/hw`).

## Hardware evidence (RP2350A, v0.1.10 DSP)
- Before, a tight pulse gave a strong bang while the gapped default was weak.
- After the fix, the armed transmit is strong and consistent: ~200 counts at
  sample ~31 across shots, vs ~30 disarmed.
- **Echo-vs-target not confirmable on the bench** (no reflector/target here) —
  the user should verify with their piezo + a reflector.

## Notes
- Default pulse order is `neg-first`; v0.1.0 was effectively `pos-first`. Order
  only inverts the bipolar waveform, so it should not affect echo presence, but
  can be matched with `pulse config ... pos-first` if desired.
- First fix logged under the new CHANGELOG rule → `firmware/CHANGELOG.md` 0.1.10.
