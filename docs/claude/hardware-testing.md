# Hardware testing — working notes

How to flash and talk to a physically-connected pic0rick board from this machine,
and the verified flash/DFU iterate loop. Written 2026-09-13 against a connected
**RP2350A-with-wifi (Pico 2 W)** dev board.

## The connected device (2026-09-13)

- Board: RP2350A with wifi = **Pico 2 W**. We build/flash the **`pico2`** target
  (not `pico2_w`) — the firmware uses no wifi/CYW43, so it runs fine. **If wifi
  is ever used, build `pico2_w` instead.**
- USB IDs:
  - Running app firmware: `2e8a:0009` ("Raspberry Pi Pico"), a USB-CDC serial
    port at **`/dev/ttyACM0`** (115200 baud).
  - BOOTSEL / bootloader ("DFU"): `2e8a:000f` ("RP2350 Boot"), a mass-storage
    device that **auto-mounts at `/media/kelu/RP2350`**.

## Flashing (UF2 → mass storage)

Device must be in BOOTSEL first (see reboot-dfu below, or hold BOOTSEL on power-up).

```bash
cp firmware/dist/pic0rick-v<ver>-rp2350-mux.uf2 /media/kelu/RP2350/ && sync
```
The device reboots into the app automatically; the CDC port reappears at
`/dev/ttyACM0` within ~1–2 s.

## Talking to the REPL

The firmware blocks on `stdio_usb_connected()` at boot, so it only starts the
`run> ` REPL once a host **opens the port with DTR asserted** (pyserial does this
by default). Minimal check:

```python
import time, serial
s = serial.Serial("/dev/ttyACM0", 115200, timeout=0.3)
time.sleep(1.0); s.reset_input_buffer()
s.write(b"version\n"); time.sleep(1.0)
print(s.read(4096).decode("utf-8","replace"))
```
Serial access works from the normal user (member of `dialout`; node is
`crw-rw----+ root dialout /dev/ttyACM0`).

## The flash/DFU iterate loop (verified 2026-09-13)

`reboot-dfu` (added fw v0.1.2) closes the loop so no BOOTSEL button press is
needed between flashes:

1. `cp <uf2> /media/kelu/RP2350/ && sync` → runs, CDC at `/dev/ttyACM0`.
2. Test over serial.
3. Send `reboot-dfu\n` → device drops to BOOTSEL (`2e8a:000f`) in ~2 s; CDC
   disappears, MSD re-mounts at `/media/kelu/RP2350`.
4. Back to step 1.

`reboot-dfu` calls `reset_usb_boot(0, 0)` (`pico/bootrom.h`, links
`pico_bootrom`; the call does not return).

## Verified on hardware (2026-09-13, fw v0.1.2, pico2/RP2350)

- `version` returned: `board: pico2 (RP2350)`, `mux: enabled (MAX14866)`,
  `build: <hash>-dirty` (dirty = built from an uncommitted tree),
  `release: .../fw-v0.1.2`.
- `reboot-dfu` reliably re-entered BOOTSEL; reflashing from `/media/kelu/RP2350`
  restored the running firmware.

## DSP build (`-DDSP=ON`, RP2350) specifics

The DSP firmware (`main_dsp.c`) uses **raw TinyUSB** (SDK stdio off), but still
enumerates as a CDC serial port at `/dev/ttyACM0`. Differences from the stdio
REPL:
- Control replies are `OK …` / `ERR <code> …` lines; there is no `run>` prompt.
- Commands: `status`, `help`, `version`, `reboot-dfu`, `pulser arm|disarm`,
  `pulse config …`, `dac write <0..1023>`, `dsp scale|selftest`,
  `acq raw|envelope|alaw`, `stream start|stop`, `write/set/clear mux` (MUX
  builds), legacy `start acq` / `read`.
- Data is returned as 64-byte-header binary frames (raw=type1 uint16,
  envelope=type2 f32, alaw=type3 u8) — e.g. `acq raw` → `OK capture started …`
  then a 64-byte header + 8192-byte payload (8256 B total for 4096 samples).
- No text is emitted while a stream is in flight.
- Verified 2026-09-13 on the RP2350A: version/help/status + `acq raw` frame.

## Gotchas

- The `-dirty` in the `build:` line just means the uf2 was built before
  committing; commit then rebuild for a clean hash.
- After `reboot-dfu`, wait for the MSD auto-mount (a couple of seconds) before
  copying the next UF2.
- Only the running app exposes the CDC; in BOOTSEL there is no `/dev/ttyACM*`.
