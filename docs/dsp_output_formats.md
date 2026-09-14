# Understanding the pic0rick firmware outputs

This note explains every output the RP2350A envelope/A-law firmware produces on
its USB-CDC link, so a human (or the capture tool) can read them without diving
into the C sources. It covers:

1. The two kinds of output (text lines vs. binary frames).
2. The `status` line, field by field.
3. The other text responses (`help`, per-command `OK`, `ERR` codes).
4. The 64-byte binary frame header and payloads.
5. The self-test output.

All field definitions come straight from the firmware: `main.c` (`send_status`,
`send_help`, command handlers), `u4rk.h` (constants), `dsp.c` (DSP stages),
`pipeline.c` (drop accounting) and `protocol.c` (`u4rk_serialize_header`).

---

## 1. Two output channels on one CDC port

The firmware never mixes the two:

- **Text mode** — every reply is a single CRLF-terminated line starting with
  `OK ` or `ERR `. This is what you get for `status`, `help`, `dac`, `pulse`,
  `dsp scale`, and the acknowledgement of `acq` / `stream start`.
- **Binary mode** — after an `acq`/`stream start`/`dsp selftest` is
  acknowledged with an `OK`, the firmware emits one or more fixed-layout
  binary **frames** (64-byte header + payload). No text is interleaved while
  frames are in flight; a stream is closed with `stream stop`, which returns to
  text mode with a final `OK stream stopped drops=N`.

---

## 2. The `status` line

Example (all on one line):

```
OK board=pic0rick package=RP2350A firmware=1.6 dsp_backend=f32-rfft-hilbert
   samples=4096 sample_rate=60000000 pulser=disarmed pulse=96/6000/96/pos-first
   dac=1023 scale=512.000 stream=off/1 drops=0
   stages_us=538/4392/137/4751/1066/689 dsp_us=11579 worst_us=12069
   performance=over-budget envelope_max_rate=50 alaw_max_rate=70 cmsis=1.17.0
```

It is a space-separated list of `key=value` tokens. Field by field:

| Token | Example | Meaning |
|---|---|---|
| `board` | `pic0rick` | Fixed board family string. |
| `package` | `RP2350A` | MCU package this build targets (Pico 2 / RP2350A). Fixed literal. |
| `firmware` | `1.6` | Firmware version (`pico_set_program_version` in `CMakeLists.txt`). The capture tool refuses any other value. |
| `dsp_backend` | `f32-rfft-hilbert` | Envelope algorithm: float32 real-FFT Hilbert transform on the Cortex-M33 FPU. Fixed literal. |
| `samples` | `4096` | Samples per acquisition (`U4RK_SAMPLE_COUNT`). |
| `sample_rate` | `60000000` | ADC sample rate in **Hz** (60 MS/s, `U4RK_SAMPLE_RATE_HZ`). |
| `pulser` | `disarmed` | `armed` or `disarmed`. A capture only fires the pulser when armed. |
| `pulse` | `96/6000/96/pos-first` | Pulser timing: `negative_ns / damp_ns / positive_ns / order`. `order` is `neg-first` or `pos-first`. Durations are the *actual* values after rounding to the 8 ns PIO tick. |
| `dac` | `1023` | Last value written to the MCP4812 DAC (0–1023, 10-bit code). Boots at 0. |
| `scale` | `512.000` | A-law full-scale reference in **ADC counts** (`dsp scale`). Envelope values at/above this map to A-law 255; default 512. |
| `stream` | `off/1` | `state/rate_hz`. `state` is `on` or `off`; `rate_hz` is the last requested stream rate. |
| `drops` | `0` | Cumulative dropped frames since boot (`processing_drops + usb_drops`, see §2.2). |
| `stages_us` | `538/4392/137/4751/1066/689` | Per-stage time of the **last DSP frame**, in **microseconds** (see §2.1). |
| `dsp_us` | `11579` | Total time of the last DSP frame, in µs. |
| `worst_us` | `12069` | Worst-case total DSP time observed, in µs (running maximum in `dsp.c`). |
| `performance` | `over-budget` | `ok` if `worst_us <= 4500`, else `over-budget` (`U4RK_DSP_TARGET_US = 4500`). |
| `envelope_max_rate` | `50` | Max accepted `stream` rate for `envelope`, in Hz. |
| `alaw_max_rate` | `70` | Max accepted `stream` rate for `alaw`, in Hz. |
| `cmsis` | `1.17.0` | CMSIS-DSP library version compiled in. |

> Raw's max stream rate (100 Hz, `U4RK_RAW_MAX_RATE_HZ`) is **not** printed in
> the status line — only `envelope_max_rate` and `alaw_max_rate` are.

### 2.1 The six DSP stages (`stages_us`)

The order is fixed: **`preprocess / forward_fft / mask / inverse_fft / magnitude / alaw`**.
They correspond one-to-one to the blocks in `u4rk_dsp_envelope()`:

| # | Stage | What happens |
|---|---|---|
| 1 | `preprocess` | Unpack the 10-bit ADC samples (`(dma >> 1) & 0x3FF`), compute the DC mean, subtract it. |
| 2 | `forward_fft` | 4096-point float32 real FFT (`arm_rfft_fast_f32`, forward). |
| 3 | `mask` | Build the analytic-signal spectrum: multiply the packed half-spectrum by `-j`. |
| 4 | `inverse_fft` | Inverse real FFT to recover the Hilbert (quadrature) component. |
| 5 | `magnitude` | `sqrt(real² + quadrature²)` per sample → the envelope; also tracks the peak. |
| 6 | `alaw` | A-law compression of the envelope (only for A-law frames; `0` otherwise). |

**Raw and legacy captures skip the FFT entirely** (they only run `preprocess`),
so after a raw/legacy capture the status reports `stages_us=0/0/0/0/0/0`,
`dsp_us=0`, and `worst_us=0`. The values persist from the *last DSP frame*, so a
plain `status` query reflects whatever job most recently completed. `worst_us`
is a running maximum kept inside the envelope backend, so it re-appears (largest
value seen since boot) after the next envelope/A-law frame.

### 2.2 Drop accounting (`drops`)

`drops` is the sum of two cumulative-since-boot counters:

- **processing drops** — a capture could not get a buffer/output slot, a DSP
  result could not be queued, or a frame was discarded during `stream stop` /
  DMA-fault cleanup.
- **USB drops** — a completed frame could not be sent to the host (transfer
  failed, or the CDC session closed with work still queued).

For a healthy stream the acceptance criterion is `drops=0`.

---

## 3. Other text responses

### `help`

One line listing every command (used by the tool to confirm there is **no**
MAX14866/mux command):

```
OK commands=status|help|pulser arm|pulser disarm|pulse config <negative_ns> <damp_ns> <positive_ns> <neg-first|pos-first>|dac write <0..1023>|dsp scale <reference>|dsp selftest|acq <raw|envelope|alaw>|stream start <raw|envelope|alaw> <rate_hz>|stream stop|start acq|read
```

### Per-command `OK` acknowledgements

| Command | Success reply |
|---|---|
| `pulser arm` / `pulser disarm` | `OK pulser armed` / `OK pulser disarmed` |
| `pulse config N D P order` | `OK pulse=N/D/P/order` (actual, tick-rounded values) |
| `dac write V` | `OK dac=V` |
| `dsp scale R` | `OK scale=R` |
| `dsp selftest` | `OK selftest frames=21 cases=7` (then 21 binary frames) |
| `acq <type>` | `OK capture started type=<type>` (then 1 binary frame) |
| `stream start <type> <rate>` | `OK stream started type=<type> rate=<rate>` (then binary frames until `stream stop`) |
| `stream stop` | `OK stream stopped drops=N` (or `OK stream already stopped` if not streaming) |
| `start acq` (legacy) | `OK legacy acquisition started` |
| `read` (legacy) | `OK raw-hex 4096` followed by 4096 comma-separated 3-hex-digit samples |

### `ERR` codes

Failures are `ERR <code> <message>`:

| Code | Meaning |
|---|---|
| `BUSY` | An operation is already in progress (no free buffer / pipeline busy). |
| `ARG` | Malformed arguments (bad type, missing/extra token, bad order keyword). |
| `RANGE` | Value out of range (DAC not 0–1023, scale not in (0, 65535], pulse below the 40 ns floor). |
| `RATE` | Requested stream rate outside `1..max` for that payload type. |
| `DMA` | Acquisition timed out; the pulser is disarmed. |
| `COMMAND` | Unknown command. |
| `LINE` | Command line too long. |
| `NO_DATA` | `read` issued before any acquisition completed. |

---

## 4. Binary frames

Every acquisition/stream/self-test result is a **64-byte little-endian header**
followed by its payload. Layout (from `u4rk_serialize_header`):

| Offset | Size | Field | Notes |
|---:|---:|---|---|
| 0 | 4 | magic | ASCII `P0RK` |
| 4 | 1 | protocol version | `1` |
| 5 | 1 | payload type | `1`=raw, `2`=envelope, `3`=A-law |
| 6 | 2 | flags | bitfield, see below |
| 8 | 4 | sequence | monotonic frame counter |
| 12 | 4 | sample count | `4096` |
| 16 | 4 | sample rate | `60000000` Hz |
| 20 | 4 | payload bytes | `8192` raw / `16384` envelope / `4096` A-law |
| 24 | 8 | capture timestamp | µs since boot (self-test: the case index) |
| 32 | 4 | ADC DC mean | float32, mean of the raw samples |
| 36 | 4 | envelope peak | float32, max envelope amplitude (0 for raw) |
| 40 | 4 | A-law reference | float32, the `scale` used for this frame |
| 44 | 4 | pulse negative_ns | uint32 |
| 48 | 4 | pulse damp_ns | uint32 |
| 52 | 4 | pulse positive_ns | uint32 |
| 56 | 4 | dropped frames | cumulative drops at frame time |
| 60 | 4 | payload CRC32 | IEEE CRC32 of the payload only |

### Payload types

| Type | dtype | Bytes | Content |
|---|---|---|---|
| `1` raw | `uint16` LE | 8192 | 10-bit ADC codes (0–1023). |
| `2` envelope | `float32` LE | 16384 | Hilbert envelope amplitude in ADC counts. |
| `3` A-law | `uint8` | 4096 | A-law-compressed positive envelope (A = 87.6). |

### Flags (offset 6)

| Bit | Name | Meaning |
|---:|---|---|
| 0 | `ALAW_SATURATED` | An A-law sample hit the `scale` ceiling and was clipped. |
| 1 | `PROCESSING_DROP` | At least one processing drop has occurred since boot. |
| 2 | `USB_DROP` | At least one USB drop has occurred since boot. |
| 3 | `SELFTEST` | This frame is a deterministic self-test vector. |
| 4 | `PULSER_ARMED` | The pulser was armed for this capture. |
| 8–15 | self-test case | For self-test frames, the case index (0–6) lives in the high byte. |

So a self-test A-law frame for case 2 (`sinusoid`) that saturated would show
`flags = 0x0209` (`0x0200` case=2, `0x08` SELFTEST, `0x01` ALAW_SATURATED).

---

## 5. Self-test output

`dsp selftest` (or the tool's `--selftest`) emits **21 frames = 7 cases × 3
payload types** (raw, envelope, A-law), in that order per case. The 7 cases are:

| Case | Name | Signal |
|---:|---|---|
| 0 | `zero` | all zero |
| 1 | `dc` | constant 700 |
| 2 | `sinusoid` | 32-cycle sine |
| 3 | `am` | amplitude-modulated tone |
| 4 | `two-bursts` | two windowed bursts |
| 5 | `impulse` | single spike at the midpoint |
| 6 | `clipping` | full-scale square wave |

The PC tool validates each case against `scipy.signal.hilbert` and prints, per
case:

```
sinusoid     nrms=8.389e-08 peak_delta=0 alaw_delta=0
```

- `nrms` — normalized RMS error of the envelope (pass ≤ `1e-4`).
- `peak_delta` — envelope peak-index difference in samples (pass ≤ 1).
- `alaw_delta` — max A-law byte-level difference vs. the reference (pass ≤ 1).

---

## Parsing helpers

The host library `python/pic0rick/dsp.py` parses this protocol:

- `pic0rick.dsp.FrameReader(port).read_frame()` → a `Frame` (header + payload;
  CRC-checked; `.samples()` decodes to a numpy array). Length-agnostic:
  `read_raw` (8000) and `read_fft` (4096) both parse.
- `pic0rick.dsp.parse_status(line)` → a typed `dict` (with `pulse`, `stream`,
  `stages_us` broken out as sub-dicts).
- `pic0rick.dsp.describe_status(line_or_dict)` → a human-readable multi-line
  summary (parameters + per-stage DSP µs times).

From the device driver: `Pic0rick.status()`, `.read_fft()`, `.read_raw()`,
`.capture(payload)` (see `docs/claude/python-host-tools.md`).

> This spec was written for the retired `experiments/onboard_dsp/` firmware; that
> firmware is now the mainline `-DDSP` build (`firmware/dsp` + `firmware/hw`).
> The formats are unchanged except that `sample_count` is now per-mode.
