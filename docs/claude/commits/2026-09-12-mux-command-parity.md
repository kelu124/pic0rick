# 2026-09-12 — Firmware↔Python MUX command parity

## What
Added `write_mux(value)`, `set_mux()`, and `clear_mux()` to the `Pic0rick`
class in `python/pic0rick/device.py`.

## Why
Audited every serial command the firmware dispatches (`firmware/main.c`
`command_list`) against the methods in `python/pic0rick`. Three commands had no
Python counterpart.

## Command audit

| Firmware command | Handler (C)                | Python `Pic0rick` method | Status  |
|------------------|----------------------------|--------------------------|---------|
| `start acq`      | `pulse_adc_trigger`        | `pulse_adc_trigger()`    | existed |
| `write dac`      | `dac`                      | `dac()`                  | existed |
| `read`           | `adc`                      | `read()`                 | existed |
| `write mux`      | `max14866` (parses hex)    | `write_mux()`            | **added** |
| `set mux`        | `max14866_set`             | `set_mux()`              | **added** |
| `clear mux`      | `max14866_clear`           | `clear_mux()`            | **added** |

## Implementation notes
- Firmware `max14866()` parses its argument with `strtol(input, NULL, 16)`, so
  `write_mux()` formats an int as an uppercase hex string; a string is passed
  through verbatim (allowing a "0x" prefix).
- `set mux` / `clear mux` take no arguments in firmware (they just pulse the
  SET / CLR GPIO lines), so the Python methods send the bare command.

## Discoveries → logged in TODO.md
- The high-level `ndt_acquisition.py` flow never touches the mux; multi-element
  scanning would need mux selection integrated.
- The MAX14866 16-bit word-to-channel bit map is undocumented; needed before
  `write_mux` is usable in practice.
