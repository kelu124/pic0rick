# Open Questions

This file collects questions and ambiguities found during a documentation review of the repo.
Some may be answerable from hardware files or Luc's notes; others may need explicit decisions.

---

## Firmware

**Q1 — DMA timeout race condition (see TODO.md)**
`TODO.md` notes that `sleep_us(1)` should be inserted after `pio_sm_put_blocking(pio_adc, sm, SAMPLE_COUNT)` to give the ADC state machine time to reach its `wait irq 0` state before the pulse fires. Is this fix ready to merge?

**Q2 — RP2350 firmware status**
The README and "Published documents" section mention `rp2040/rp2350 firmware`. Is the RP2350 build tested and confirmed working? Are there any differences from the RP2040 build (clock dividers, PIO programs, DMA channels)?

**Q3 — Maximum `pon`/`poff`/`damp` values**
What are the hardware-safe upper limits for `pon`, `poff`, and `damp` (nanoseconds)? The firmware accepts any value that fits in an `int`, but the pulser board may have limits (inductor saturation, HV cap discharge time).

**Q4 — `SAMPLE_COUNT` configurability**
`SAMPLE_COUNT` is hardcoded to 8000 in `adc.h`. Can it be changed via a serial command, or does it require a firmware rebuild?

---

## Hardware

**Q5 — HV rail voltage tolerance**
The IAS0105D24 datasheet specifies ±24V output. The board schematic shows no monitoring or feedback path. Is the actual rail voltage measured/characterised across production boards? What is the typical spread (e.g. ±22–26V)?

**Q6 — HV board OE signal**
The KiCad schematic shows a `OE` global label on J5 pin 3. Is this active-high or active-low? Is it driven from the RP2040, or tied to a static level?

**Q7 — Production PCB vs GitHub KiCad files**
Support emails suggest the production board differs from the KiCad files in the current `main` branch. The production version appears to be at commit `bab83371f452f650acd79c82d3ccac702bd4d4ba`. Should the README note the production-tag explicitly, or should the main-branch files be updated?

**Q8 — MUX board documentation**
`write mux`, `set mux`, and `clear mux` commands are implemented in firmware (MAX14866), and a "KiCad design files for the MUX" entry appears in the README. Where is that KiCad file? There is no `hardware/mux/` directory in the repo.

---

## Python library

**Q9 — DAC value to dB mapping**
`gain` is stored and passed as a raw 10-bit DAC value (0–1023). The AD8331 VGAIN pin maps voltage to gain in dB (linear: 0 V → 7.5 dB, 1 V → 55.5 dB). The MCP4812 VOUT depends on VREF. What is VREF on this board? This would let us publish a `dac_to_db(n)` helper and label plots in dB rather than raw counts.

**Q10 — `Fech` hardcoded vs measured**
`Fech = 60e6` is set in `device.py` and passed through to every `UltrasonicAcquisition`. It is derived from `ADC_CLK = 120 MHz` with 2 PIO cycles per sample. Has this been verified with a frequency counter or known-thickness calibration? Is there a trim/calibration path if the actual rate differs?

**Q11 — `requirements.txt` scope**
The existing `requirements.txt` is a full `pip freeze` (122 packages including Jupyter, Streamlit, etc.). The new `requirements-minimal.txt` lists only the five packages actually imported by `pic0lib`. Should `requirements.txt` be replaced by the minimal file, or kept as a "dev environment" freeze alongside the minimal one?

---

## Documentation

**Q12 — Assembly instructions**
There are no step-by-step assembly instructions for the three-board stack (main + pulser + HV). The `documentation/images/` folder has photos but no reference designator callouts. Is a build guide planned?

**Q13 — VGA branch maintenance**
The README mentions a VGA proof-of-concept on the `VGA` branch. Is that branch actively maintained? Should it be flagged as experimental/archived?

**Q14 — Transducer compatibility**
The example notebook uses a 5 MHz contact probe on 1018 steel. What other transducer frequencies/types have been tested? Are immersion probes supported (any hardware changes needed)?
