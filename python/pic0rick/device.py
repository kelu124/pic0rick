# Main libs
import os
import re
import sys
import glob
import time
import struct
# Third party
import serial
import numpy as np


def _find_port():
    if sys.platform.startswith("win"):
        ports = glob.glob("COM[0-9]*")
    elif sys.platform.startswith("linux"):
        ports = glob.glob("/dev/ttyACM*") + glob.glob("/dev/ttyUSB*")
    elif sys.platform.startswith("darwin"):  # macOS
        ports = glob.glob("/dev/tty.usbmodem*") + glob.glob("/dev/tty.usbserial*")
    else:
        raise OSError(f"Unsupported platform: {sys.platform}")

    if not ports:
        raise OSError("No serial device found")
    return ports[0]


def pprint(ans):
    return "".join(b.decode("utf-8") for b in ans)


class Pic0rick:

    def sread(self):
        done = False
        ans = []
        while not done:
            res = self.ser.readline()
            ans.append(res)
            if res == b"":
                done = True
        if self.verbose:
            pprint(ans)
        if self.log:
            with open(self.log_file, "a") as f:
                for line in ans:
                    if line and line != b"":
                        f.write(line.decode("utf-8", errors="replace"))
        return ans

    def __init__(self, port=None, verbose=True, logging=False, log_file=".log"):
        self.verbose = verbose
        self.log = logging
        self.log_file = log_file

        # Detect the port
        port_device = port if port is not None else _find_port()
        print("Device on", port_device)

        self.ser = serial.Serial(port_device, 115200, timeout=0.2)
        self.ser.baudrate = 115200
        time.sleep(1)  # wait for the serial connection to initialize
        self.sread()

        self.Fech = 60e6  # ADC sampling frequency (Hz)


    def dac(self, N):
        """Write a value to the 10-bit DAC MCP4812 (write dac).

        Args:
            N: DAC value (10-bit).
        """
        self.ser.write(bytearray("write dac " + str(N) + "\n", "ascii"))
        ans = self.sread()
        return ans
    
    def read(self):
        self.ser.write(bytearray("read\n", "ascii"))
        ans = self.sread()
        return ans

    def version(self):
        """Query the firmware `version` command and parse its report.

        Returns a dict with whatever the firmware reported. Keys (all optional,
        present only if the firmware emits the corresponding line):
            version : firmware version string, e.g. "0.1.1"
            changes : one-line change summary
            board   : PICO_BOARD, e.g. "pico2"
            chip    : e.g. "RP2350"
            mux     : raw mux line, e.g. "enabled (MAX14866)"
            mux_enabled : bool derived from `mux`
            build   : git build hash, e.g. "b0d9435" or "b0d9435-dirty"
            release : GitHub release URL for this version
            raw     : the full decoded response text
        """
        self.ser.write(bytearray("version\n", "ascii"))
        text = pprint(self.sread())
        info = {"raw": text}

        m = re.search(r"firmware\s+v(\d+\.\d+\.\d+)", text)
        if m:
            info["version"] = m.group(1)

        for line in text.splitlines():
            line = line.strip()
            if line.startswith("changes:"):
                info["changes"] = line.split(":", 1)[1].strip()
            elif line.startswith("board:"):
                rest = line.split(":", 1)[1].strip()
                bm = re.match(r"(\S+)(?:\s*\(([^)]*)\))?", rest)
                if bm:
                    info["board"] = bm.group(1)
                    if bm.group(2):
                        info["chip"] = bm.group(2)
            elif line.startswith("mux:"):
                mux = line.split(":", 1)[1].strip()
                info["mux"] = mux
                info["mux_enabled"] = mux.lower().startswith("enabled")
            elif line.startswith("build:"):
                info["build"] = line.split(":", 1)[1].strip()
            elif line.startswith("release:"):
                info["release"] = line.split(":", 1)[1].strip()

        return info
    
    def pulse_adc_trigger(self, pon: int=200,poff:int=200,damp:int=2000):
        self.ser.write(bytearray("start acq "+str(pon)+" "+str(poff)+" "+str(damp)+"\n",'ascii'))
        ans = self.sread()
        return ans

    def write_mux(self, value):
        """Load the MAX14866 mux shift register (write mux).

        Firmware parses the argument as hexadecimal (strtol base 16), so an int
        is sent as an uppercase hex string. A string is passed through verbatim,
        allowing an explicit "0x" prefix if desired.

        Args:
            value: mux pattern as an int, or a hex string (e.g. 0xFFFF or "FFFF").
        """
        arg = format(value, "X") if isinstance(value, int) else str(value)
        self.ser.write(bytearray("write mux " + arg + "\n", "ascii"))
        ans = self.sread()
        return ans

    def set_mux(self):
        """Pulse the MAX14866 SET line, enabling all switches (set mux)."""
        self.ser.write(bytearray("set mux\n", "ascii"))
        ans = self.sread()
        return ans

    def clear_mux(self):
        """Pulse the MAX14866 CLR line, disabling all switches (clear mux)."""
        self.ser.write(bytearray("clear mux\n", "ascii"))
        ans = self.sread()
        return ans

    # ------------------------------------------------------------------
    # DSP-build (-DDSP, RP2350) helpers. These speak the binary framed
    # protocol; use them only against a DSP firmware build (see status()).
    # ------------------------------------------------------------------

    def status(self):
        """Query the DSP-build `status` line and return it parsed (dict).

        Raises ValueError against a non-DSP firmware (no status line).
        """
        from pic0rick import dsp
        self.ser.reset_input_buffer()
        self.ser.write(b"status\n")
        line = self.ser.readline().decode("utf-8", "replace")
        info = dsp.parse_status(line)
        info["raw"] = line.strip()  # the Pico's exact status text
        return info

    def capture(self, payload="envelope"):
        """Trigger a one-shot DSP capture and return the parsed `dsp.Frame`.

        Args:
            payload: 'raw' (read_raw, 8000 samples), 'envelope' (read_fft, 4096)
                     or 'alaw' (4096). DSP firmware only.
        """
        from pic0rick import dsp
        command = {"raw": "read_raw", "envelope": "read_fft",
                   "alaw": "acq alaw"}.get(payload)
        if command is None:
            raise ValueError("payload must be raw, envelope or alaw")
        self.ser.reset_input_buffer()
        self.ser.write((command + "\n").encode("ascii"))
        # The firmware sends an "OK capture started ..." text line before the
        # binary frame; capture it (self.last_reply) so callers can display it.
        self.last_reply = self.ser.readline().decode("utf-8", "replace").strip()
        return dsp.FrameReader(self.ser).read_frame()

    def read_fft(self):
        """One-shot 4096-sample Hilbert-envelope capture (DSP firmware)."""
        return self.capture("envelope")

    def read_raw(self):
        """One-shot 8000-sample raw ADC capture as a frame (DSP firmware)."""
        return self.capture("raw")

