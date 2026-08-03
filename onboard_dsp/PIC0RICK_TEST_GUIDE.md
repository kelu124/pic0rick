# pic0rick Pico 2 / RP2350A firmware test guide

This guide tests the envelope/A-law firmware itself. It assumes the included
`pic0rick-envelope.uf2` (or the equivalent VS Code build output) has already
been loaded onto the pic0rick.

## What this build controls

| Function | RP2350A GPIO | Schematic signal |
|---|---:|---|
| ADC clock | 0 | ADC_CLK |
| ADC data | 1–10 | D0–D9 |
| PMOD pulser | 11 | P+ |
| PMOD pulser | 12 | P- |
| MCP4812 | 13 | CS_DAC |
| MCP4812 | 14 | SCLK |
| MCP4812 | 15 | MOSI |
| PMOD pulser | 16 | PDAMP |
| PMOD pulser | 17 | OE |

GPIO18–21 and GPIO28, formerly associated with MAX14866 control, are not
initialized by this firmware. There is no MAX14866 command.

The four pulser outputs are low at boot. Captures do not generate a pulse
until `pulser arm` is explicitly sent. A DMA timeout, `stream stop`, USB CDC
disconnect, or reset disarms the pulser and drives all four outputs low.

## PC setup

From this folder, create an environment and install the capture dependencies:

```powershell
py -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install -r tools\requirements.txt
```

Replace `COM7` in the examples with the board's USB serial port. The `115200`
setting is conventional; USB CDC itself is not baud-rate limited.

For individual text commands, use a serial terminal such as:

```powershell
python -m serial.tools.miniterm COM7 115200 --eol LF
```

Exit miniterm with `Ctrl+]`. Do not keep a terminal and the capture script open
on the same COM port at the same time.

## 1. Boot and command check

Keep the high-voltage pulser supply disabled for initial checks. Open the
serial terminal and send:

```text
status
help
```

Expected `status` fields include:

```text
board=pic0rick package=RP2350A firmware=1.5 dsp_backend=f32-rfft-hilbert samples=4096 sample_rate=60000000 pulser=disarmed
```

Expected `help` output lists acquisition, streaming, DSP, DAC, and pulser
commands, and does not list a MAX14866 or mux command.

## 2. DSP self-test

Close the terminal, then run:

```powershell
python tools\pic0rick_capture.py --port COM7 --selftest --timeout 30 --output captures\selftest
```

The firmware produces raw, envelope, and A-law output for seven deterministic
signals: zero, DC, sinusoid, amplitude-modulated tone, two bursts, impulse,
and clipping. The PC tool checks every binary header and CRC, verifies sequence
numbers, and compares the firmware with `scipy.signal.hilbert`. It first checks
that the board reports firmware `1.5`; an older UF2 is rejected.

Pass criteria are:

- 21 frames received with no CRC or sequence error.
- Normalized envelope RMS error is at most `1e-4`.
- Peak index differs by at most one sample.
- A-law differs from the Python reference by at most one byte level.
- The command exits with code 0 and prints no `ERROR` line.

Files written to `captures\selftest` are `raw.npy`, `envelope.npy`,
`alaw.npy`, `alaw_decoded.npy`, and `headers.json`.

## 3. ADC and physical-signal checks

With the pulser disarmed, capture the ADC input in each format:

```powershell
python tools\pic0rick_capture.py --port COM7 --mode raw --output captures\raw
python tools\pic0rick_capture.py --port COM7 --mode envelope --output captures\envelope
python tools\pic0rick_capture.py --port COM7 --mode alaw --output captures\alaw
```

Each command requests a new physical acquisition; the three files are not the
same capture. Use the raw capture to check ADC range and clock/data wiring. Use
a known tone or burst to check that the saved envelope follows its amplitude.
`headers.json` records the ADC mean, envelope peak, A-law reference, pulse
configuration, flags, drop counter, and payload CRC.

Set the A-law full-scale reference, in ADC counts, before an A-law capture:

```text
dsp scale 512
```

The default after every reboot is 512 ADC counts. Values above the reference
are clipped and set flag bit 0 (`ALAW_SATURATED`) in the binary header.

## 4. DAC check

With an oscilloscope or voltmeter on the appropriate analog test point, send:

```text
dac write 0
dac write 512
dac write 1023
```

Each valid command returns `OK dac=<value>`. Check GPIO13 for active-low chip
select, GPIO14 for the 2 MHz mode-0 clock, and GPIO15 for MOSI if the analog
level is not correct.

## 5. Pulser logic check

Keep high voltage disabled and inspect GPIO11, GPIO12, GPIO16, and GPIO17 with
a logic analyzer. Configure and arm the pulser:

```text
pulse config 96 6000 96 neg-first
pulser arm
acq raw
```

For negative-first order, expect:

1. GPIO12 P- and GPIO17 OE high for 96 ns.
2. GPIO16 PDAMP and GPIO17 OE high for 6000 ns.
3. GPIO11 P+ and GPIO17 OE high for 96 ns.
4. All four signals low after the sequence.

Test the other order with:

```text
pulse config 96 6000 96 pos-first
acq raw
pulser disarm
```

P+ and P- exchange positions in the sequence. Durations are rounded to the
nearest 8 ns PIO tick. The minimum accepted duration for each stage is 40 ns.
Confirm the all-low state after `pulser disarm`, USB disconnect, and reset
before enabling high voltage.

## 6. Streaming and RP2350 timing

The compiled limits are raw 100 Hz, float envelope 50 Hz, and A-law 70 Hz.
Higher requested rates return `ERR RATE` instead of being accepted silently.
The 70 Hz A-law limit is based on the measured 12.98 ms worst case from the
same RFFT backend on a Pico 2 W; it must still be verified on this pic0rick.

Start with one frame per second:

```powershell
python tools\pic0rick_capture.py --port COM7 --mode alaw --rate 1 --frames 60 --output captures\alaw-1hz
```

Then run a 60-second test at the compiled A-law limit:

```powershell
python tools\pic0rick_capture.py --port COM7 --mode alaw --rate 70 --frames 4200 --timeout 30 --output captures\alaw-70hz
```

The tool rejects CRC errors and sequence gaps. After stopping the stream it
saves `status.txt`; require `drops=0`. The status line also reports these
measured stages in microseconds:

```text
stages_us=preprocess/forward_fft/mask/inverse_fft/magnitude/alaw
dsp_us=<last total> worst_us=<worst total>
envelope_max_rate=50 alaw_max_rate=70
```

Require zero drops, sequence gaps, and CRC errors. The original exact-Hilbert
200 Hz/4.5 ms target remains unmet; `performance=over-budget` is expected when
`worst_us` is greater than 4500.

## Commands

```text
status
help
pulser arm
pulser disarm
pulse config <negative_ns> <damp_ns> <positive_ns> <neg-first|pos-first>
dac write <0..1023>
dsp scale <reference>
dsp selftest
acq <raw|envelope|alaw>
stream start <raw|envelope|alaw> <rate_hz>
stream stop
start acq
read
```

Commands return `OK ...` or `ERR <code> <message>`. After `stream start` is
acknowledged, output is binary-framed until `stream stop`; do not interpret it
as terminal text.

## Binary frame summary

Every result begins with a fixed 64-byte little-endian header followed by its
payload. The magic is `P0RK`, protocol version is 1, and payload types are
1=raw uint16, 2=envelope float32, and 3=A-law uint8. The header contains the
sequence, 4096-sample count, 60 MHz sample rate, payload length, timestamp,
ADC mean, envelope peak, A-law reference, pulse durations, cumulative drops,
and IEEE CRC32 of the payload. The Python tool parses and validates these
fields automatically.
