# 2026-09-13 — Migration plan: unify onboard_dsp into firmware (branch)

Branch `feature/unify-firmware-dsp`. Docs only — no code, no version bump.

## What
Added `docs/claude/unify-firmware-dsp-plan.md`: an 8-phase plan to fold the
`experiments/onboard_dsp/` DSP features into `firmware/` behind a `-DDSP` flag
(RP2350 only), retiring onboard_dsp when done.

## Why
User confirmed the approach (fold + gate + CLI commands + SDK 2.3.0 + retire)
and asked for a plan.

## Key finding driving the plan
onboard_dsp uses **raw TinyUSB** (SDK stdio disabled) so text + binary frames
share one CDC; the mainline REPL uses SDK stdio. Only one USB stack can own the
CDC, so the unified firmware must choose. Recommended **Design A**: the DSP build
adopts onboard_dsp's raw-TinyUSB transport + its (already CLI-shaped) command
loop; RP2040 keeps SDK stdio. Awaiting user confirmation on this and 3 other
open items (DAC unify, pulser pin map, version handshake) before Phase 3/4.
