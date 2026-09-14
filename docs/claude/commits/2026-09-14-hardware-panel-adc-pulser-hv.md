# 2026-09-14 — hardware: register panel_adc_pulser_hv in build.sh

Branch `feature/unify-firmware-dsp`.

## What
- Added `panel_adc_pulser_hv` to the `BOARDS` registry in `hardware/build.sh`
  so `./build.sh all` includes it.
- New board folder `hardware/panel_adc_pulser_hv/` (`3_in_1_Panel.kicad_pcb` +
  `.kicad_sch`, 4-layer: F/B + In1/In2).
- Generated production outputs under
  `hardware/panel_adc_pulser_hv/build_jlcpcb/` (same layout as the other
  boards, which keep their build outputs in git).

## Why
User: include panel_adc_pulser_hv in build.sh and run it.

## Verified
`./build.sh panel_adc_pulser_hv` (group `all`) completed cleanly — schematic
present, so full set built:
- gerbers (4 copper layers, mask, silk, edge cuts, PTH/NPTH drills)
- production `3_in_1_Panel-JLCPCB-gerbers.zip`
- assembly JLCPCB BOM + CPL (CSV)
- docs schematic PDF + interactive iBOM HTML
- 3D STEP + top/bottom PNG renders
KiBot exited 0 (no warnings); both renders succeeded.
