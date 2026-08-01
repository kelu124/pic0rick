# pic0rick RP2040 envelope firmware

This folder is the complete source package for the original RP2040-based
pic0rick board. It captures 4,096 ADC samples at 60 MS/s, calculates the
Hilbert envelope, and can compress the positive envelope to 8-bit A-law.

The MAX14866 is intentionally not initialized or controlled by this build.
The firmware uses the pic0rick schematic connections directly, so it does not
need a custom board-definition header or any jumper wires.

## Build

Open this folder itself in VS Code, not its parent folder. With the official
Raspberry Pi Pico extension installed, run **Configure CMake**, then
**Compile Project**. The first configure needs Internet access so CMake can
fetch the pinned CMSIS-DSP v1.17.0 source.

The project forces `PICO_BOARD=pico`, which selects the RP2040 target even if
another board was selected previously in the extension. The output file is:

`build/pic0rick-envelope.uf2`

See `PIC0RICK_TEST_GUIDE.md` for the firmware-specific checks and commands.

## Included files

- `pic0rick/`: firmware sources, PIO programs, USB CDC device implementation.
- `tools/pic0rick_capture.py`: PC capture, CRC checking, saving, A-law decode,
  and SciPy self-test validation.
- `tools/requirements.txt`: Python packages required by the capture tool.
- `pico_sdk_import.cmake`: Pico SDK CMake integration.

Build products, VS Code machine-local settings, and unrelated RP2350 or
MAX14866 sources are deliberately not included.

