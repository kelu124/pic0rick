# pic0rick Pico 2 / RP2350A envelope firmware

This folder targets the client's pic0rick fitted with a pin-compatible
Raspberry Pi Pico 2 (RP2350A). It captures 4,096 ADC samples at 60 MS/s,
calculates the Hilbert envelope, and can compress the positive envelope to
8-bit A-law. The exact envelope backend uses a 4,096-point float32 real FFT
and the RP2350 Cortex-M33 floating-point unit.

The MAX14866 is intentionally not initialized or controlled by this build.
The firmware uses the pic0rick schematic connections directly, so it does not
need a custom board-definition header or any jumper wires.

## Build

Open this folder itself in VS Code, not its parent folder. With the official
Raspberry Pi Pico extension installed, run **Configure CMake**, then
**Compile Project**. The first configure needs Internet access so CMake can
fetch the pinned CMSIS-DSP v1.17.0 source.

The project forces `PICO_BOARD=pico2`, matching the client's RP2350A module
even if another board was previously selected in the extension. This image
also runs on Pico 2 W because the firmware does not use Wi-Fi, CYW43, or the
onboard LED. The VS Code build output is:

`build/pic0rick-envelope.uf2`

A verified prebuilt copy is also included at the package root as
`pic0rick-envelope.uf2`.

See `PIC0RICK_TEST_GUIDE.md` for the firmware-specific checks and commands.

## Included files

- `pic0rick/`: firmware sources, PIO programs, USB CDC device implementation.
- `tools/pic0rick_capture.py`: PC capture, CRC checking, saving, A-law decode,
  and SciPy self-test validation.
- `tools/requirements.txt`: Python packages required by the capture tool.
- `pico_sdk_import.cmake`: Pico SDK CMake integration.

The generated build directory, VS Code machine-local state, and unrelated
Ultr4rick RP2350B or MAX14866 sources are deliberately not included.
