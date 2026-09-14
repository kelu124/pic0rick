# pic0rick DSP firmware — test guide

How to build, flash and exercise the **`-DDSP`** firmware (RP2350 on-board
Hilbert-envelope DSP + binary protocol). For the frame/status wire formats see
[`dsp_output_formats.md`](dsp_output_formats.md); for the host API see
[`claude/python-host-tools.md`](claude/python-host-tools.md).

## 1. Build

```bash
cd firmware && ./build.sh          # all 6 variants -> firmware/dist/
# the DSP UF2s are:
#   dist/pic0rick-v<ver>-rp2350-mux-dsp.uf2
#   dist/pic0rick-v<ver>-rp2350-nomux-dsp.uf2
```
(Or build just one: `cmake -B build -DPICO_BOARD=pico2 -DDSP=ON && cmake --build build`.)

## 2. Flash

1. Enter the bootloader: hold **BOOTSEL** while plugging in USB, **or** — if a
   pic0rick firmware (≥ v0.1.2) is already running — send `reboot-dfu` over
   serial. The board mounts as an `RP2350` mass-storage drive.
2. Copy the DSP UF2 onto it; the board reboots and enumerates as a USB-CDC port
   (`/dev/ttyACM*` on Linux).

## 3. Talk to it — serial

The DSP build replies with `OK …` / `ERR …` text lines and streams binary frames
for captures. Useful commands:

| Command | Effect |
|---|---|
| `version` | firmware version, board/chip, options, git hash, release URL |
| `status` | one-line machine-readable state (see dsp_output_formats.md) |
| `help` | list all commands |
| `read_raw` | capture 8000 raw ADC samples → one raw frame |
| `read_fft` | capture 4096-sample Hilbert envelope → one envelope frame |
| `acq raw\|envelope\|alaw` | one capture of the given type |
| `dsp selftest` | stream the deterministic self-test vectors |
| `dsp scale <ref>` | set the A-law full-scale reference |
| `pulser arm` / `pulser disarm` | enable/disable firing on capture |
| `pulse config <neg_ns> <damp_ns> <pos_ns> <neg-first\|pos-first>` | pulse timing |
| `write dac <0..1023>` | set TGC gain (spi1 MCP4812) |
| `reboot-dfu` | reboot into the bootloader for reflashing |

Because captures are **binary** frames, don't parse them by hand in a plain
terminal — use the Python host below.

## 4. Talk to it — Python

```python
from pic0rick.device import Pic0rick
p = Pic0rick(verbose=False)          # auto-detects the port

print(p.status())                    # parsed dict: firmware, samples, pulser, ...

frame = p.read_fft()                 # 4096-sample Hilbert envelope
env = frame.samples()                # numpy float32[4096]; CRC already checked
print("envelope peak", env.max())

raw = p.read_raw().samples()         # numpy uint16[8000]
```

`p.capture("raw"|"envelope"|"alaw")` is the general form; `read_raw()` /
`read_fft()` are shortcuts. Frame metadata is on `frame.header`
(`sample_count`, `payload_bytes`, `adc_dc_mean`, `envelope_peak`, `pulse`, …).
A-law payloads can be expanded with `pic0rick.dsp.alaw_decode(bytes, reference)`.

## 5. Run the example

`python/example_dsp.py` ties it together (status → read_raw → read_fft →
a pulsed capture):

```bash
cd python
python example_dsp.py                 # summary to stdout
python example_dsp.py --save out/     # + raw.npy / envelope.npy
python example_dsp.py --plot out/     # + capture.png (needs matplotlib)
```

For an interactive, step-by-step version, open the notebook
`python/example_dsp.ipynb` and run the cells top to bottom (set `PORT` in cell 2).
It arms the pulser from the start and covers **raw / Hilbert envelope / A-law**
(with `alaw_decode` overlaid on the envelope), the **DSP self-test**, a
**pulser-order** check, shows every Pico text reply + per-stage DSP µs times, and
**saves each capture** to `captures/<name>/` (`<payload>.npy` + `header.json` +
`status.txt`). `captures/` is gitignored.

Expected output resembles:

```
Board: pic0rick RP2350A | firmware 0.1.9 | backend f32-rfft-hilbert
Captures (pulser disarmed):
  read_raw  type=raw      samples= 8000 bytes=16000 seq=...
            min/mean/max = .../.../...
  read_fft  type=envelope samples= 4096 bytes=16384 seq=...
            envelope peak=... dc_mean=...
Captures (pulser armed):
  read_fft  type=envelope samples= 4096 ...
```

## 6. Self-test (optional)

`dsp selftest` streams 7 cases × 3 payload types (raw/envelope/A-law) of known
synthetic vectors — a firmware-only integrity check of the DSP pipeline. Read the
frames with a `pic0rick.dsp.FrameReader(p.ser)` loop; each frame's
`header.selftest_case` identifies the vector.
