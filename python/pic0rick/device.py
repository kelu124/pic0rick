# Main libs
import os
import re
import sys
import glob
import time
import struct
import zlib
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
        # Firmware >= 0.1.12 prints "pic0rick firmware vX uid: <id>" on connect.
        banner = pprint(self.sread())
        m = re.search(r"uid:\s*([0-9A-Fa-f]+)", banner)
        self.uid = m.group(1) if m else None

        self.Fech = 60e6  # ADC sampling frequency (Hz)

        # Which firmware flavour is running: the DSP build (-DDSP) answers
        # `status`; the default stdio build (rp2040.uf2 / rp2350.uf2) does not.
        # The high-level helpers below (set_gain, configure_pulse, arm_pulser,
        # read_raw, ...) work on both, picking the right CLI.
        self.is_dsp = self._detect_dsp()
        self._pulse = (200, 2000, 200)  # stdio: (negative, damp, positive) ns
        self._armed = False
        self.last_reply = ""
        print("Firmware flavour:", "DSP" if self.is_dsp else "stdio")
        if self.uid is None:  # banner already consumed by an earlier session
            self.uid = self.version().get("uid")
        print("Board UID:", self.uid or "unknown (firmware < 0.1.12)")

    def _detect_dsp(self):
        # A freshly enumerated port may already hold a partial junk line on the
        # device (e.g. host modem probing); terminate and discard it first.
        self.ser.write(b"\n")
        self.sread()
        self.ser.reset_input_buffer()
        self.ser.write(b"status\n")
        text = pprint(self.sread())
        return "Unknown command" not in text and "status" in text.lower()


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
            uid     : Pico unique board ID, 16 hex chars (fw >= 0.1.12)
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
            if line.startswith("uid:"):
                info["uid"] = line.split(":", 1)[1].strip()
            elif line.startswith("changes:"):
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
        """One-shot 8000-sample raw ADC capture as a `dsp.Frame`.

        Works on both builds (stdio: `start acq` + `read`).
        """
        if not self.is_dsp:
            return self._stdio_read_raw()
        return self.capture("raw")

    def set_gain(self, value):
        """Set the TGC gain DAC, 0..1023.

        DSP build: `dac write <n>`; stdio build: `write dac <n>`.
        """
        if not self.is_dsp:
            return self.dac(int(value))
        self.ser.write(("dac write %d\n" % int(value)).encode("ascii"))
        return self.sread()

    def configure_pulse(self, negative_ns, damp_ns, positive_ns,
                        order="pos-first"):
        """Configure the pulser.

        DSP build: `pulse config ...`. The stdio build has no separate config
        command, so the timings are stored and sent with the next `start acq`
        (stdio is always positive-first).
        """
        if not self.is_dsp:
            if order != "pos-first":
                raise ValueError("stdio firmware only supports order='pos-first'")
            self._pulse = (int(negative_ns), int(damp_ns), int(positive_ns))
            return []
        self.ser.write(
            ("pulse config %d %d %d %s\n"
             % (int(negative_ns), int(damp_ns), int(positive_ns), order))
            .encode("ascii"))
        return self.sread()

    def arm_pulser(self):
        """Arm the pulser so the next capture transmits."""
        self._armed = True
        if not self.is_dsp:
            return []  # stdio `start acq` arms + disarms around each capture
        self.ser.write(b"pulser arm\n")
        return self.sread()

    def disarm_pulser(self):
        """Disarm the pulser."""
        self._armed = False
        if not self.is_dsp:
            return []
        self.ser.write(b"pulser disarm\n")
        return self.sread()

    def _stdio_read_raw(self):
        """stdio build: `start acq` + `read`, wrapped as a raw `dsp.Frame`."""
        from pic0rick import dsp
        negative_ns, damp_ns, positive_ns = self._pulse
        self.ser.reset_input_buffer()
        self.ser.write(("start acq %d %d %d\n"
                        % (positive_ns, negative_ns, damp_ns)).encode("ascii"))
        text = pprint(self.sread())
        if "Acquisition ended" not in text:
            raise RuntimeError("acquisition failed: " + text.strip())
        self.last_reply = "Acquisition ended"

        self.ser.write(b"read\n")
        lines, deadline = [], time.time() + 5
        while time.time() < deadline:
            line = self.ser.readline().decode("utf-8", "replace")
            if "End of ACQ" in line:
                break
            lines.append(line)
        else:
            raise TimeoutError("no end-of-acquisition marker from `read`")
        data = "".join(lines).split("Start of ACQ")[-1].strip("-\r\n ")
        samples = np.array([int(x, 16) for x in data.split(",") if x.strip()],
                           dtype="<u2")
        payload = samples.tobytes()
        header = dsp.FrameHeader(
            version=dsp.PROTOCOL_VERSION, payload_type=1, flags=0, sequence=0,
            sample_count=len(samples), sample_rate_hz=int(self.Fech),
            payload_bytes=len(payload), capture_timestamp_us=0,
            adc_dc_mean=float(samples.mean()) if len(samples) else 0.0,
            envelope_peak=0.0, alaw_reference=0.0, negative_ns=negative_ns,
            damp_ns=damp_ns, positive_ns=positive_ns, dropped_frames=0,
            payload_crc32=zlib.crc32(payload))
        return dsp.Frame(header=header, payload=payload)
