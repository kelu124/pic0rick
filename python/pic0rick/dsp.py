"""pic0rick DSP-build host protocol.

Parses the binary framed protocol emitted by the RP2350 DSP firmware
(``-DDSP`` build): a 64-byte little-endian header + CRC32 payload, plus the
``OK …`` / ``ERR …`` text control lines and the ``status`` line.

Ported from the former onboard_dsp capture tool and made
**length-agnostic**: the unified firmware uses per-mode sample counts
(``read_raw`` = 8000, ``read_fft`` = 4096), so payload sizes are derived from the
header's ``sample_count`` rather than a fixed constant.
"""

import dataclasses
import struct
import zlib

import numpy as np

MAGIC = b"P0RK"
PROTOCOL_VERSION = 1
HEADER = struct.Struct("<4sBBHIIIIQfffIIIII")

PAYLOAD_NAMES = {1: "raw", 2: "envelope", 3: "alaw"}
PAYLOAD_ITEMSIZE = {1: 2, 2: 4, 3: 1}  # bytes per sample by payload type
A_LAW_A = 87.6

FLAG_ALAW_SATURATED = 1 << 0
FLAG_PROCESSING_DROP = 1 << 1
FLAG_USB_DROP = 1 << 2
FLAG_SELFTEST = 1 << 3
FLAG_PULSER_ARMED = 1 << 4
SELFTEST_CASE_SHIFT = 8
SELFTEST_NAMES = (
    "zero", "dc", "sinusoid", "am", "two-bursts", "impulse", "clipping",
)

MAX_SAMPLE_COUNT = 8000  # raw depth; envelope/alaw use 4096

STATUS_STAGE_NAMES = (
    "preprocess", "forward_fft", "mask", "inverse_fft", "magnitude", "alaw",
)


def expected_payload_bytes(payload_type: int, sample_count: int) -> int:
    return sample_count * PAYLOAD_ITEMSIZE[payload_type]


@dataclasses.dataclass(frozen=True)
class FrameHeader:
    version: int
    payload_type: int
    flags: int
    sequence: int
    sample_count: int
    sample_rate_hz: int
    payload_bytes: int
    capture_timestamp_us: int
    adc_dc_mean: float
    envelope_peak: float
    alaw_reference: float
    negative_ns: int
    damp_ns: int
    positive_ns: int
    dropped_frames: int
    payload_crc32: int

    @property
    def payload_name(self) -> str:
        return PAYLOAD_NAMES.get(self.payload_type, f"type{self.payload_type}")

    @property
    def selftest_case(self):
        if not (self.flags & FLAG_SELFTEST):
            return None
        return (self.flags >> SELFTEST_CASE_SHIFT) & 0xFF


@dataclasses.dataclass(frozen=True)
class Frame:
    header: FrameHeader
    payload: bytes

    def samples(self) -> np.ndarray:
        """Decode the payload to a numpy array (uint16 raw, float32 envelope,
        uint8 A-law)."""
        if self.header.payload_type == 1:
            return np.frombuffer(self.payload, dtype="<u2").copy()
        if self.header.payload_type == 2:
            return np.frombuffer(self.payload, dtype="<f4").copy()
        return np.frombuffer(self.payload, dtype=np.uint8).copy()


class FrameReader:
    """Buffered reader that discards text/noise until the next P0RK frame.

    Wraps any stream exposing ``read()`` (and optionally ``in_waiting``), e.g. a
    ``serial.Serial`` instance.
    """

    def __init__(self, stream, read_size: int = 4096):
        self.stream = stream
        self.read_size = read_size
        self.buffer = bytearray()

    def _fill(self, needed: int) -> None:
        while len(self.buffer) < needed:
            missing = needed - len(self.buffer)
            available = int(getattr(self.stream, "in_waiting", 0) or 0)
            request = min(self.read_size, max(missing, available))
            chunk = self.stream.read(request)
            if not chunk:
                raise TimeoutError(
                    f"timed out with {len(self.buffer)}/{needed} bytes")
            self.buffer.extend(chunk)

    def read_frame(self) -> Frame:
        while True:
            location = self.buffer.find(MAGIC)
            if location >= 0:
                del self.buffer[:location]
                break
            if len(self.buffer) > len(MAGIC) - 1:
                del self.buffer[: -(len(MAGIC) - 1)]
            self._fill(len(self.buffer) + 1)

        self._fill(HEADER.size)
        values = HEADER.unpack_from(self.buffer)
        (magic, version, payload_type, flags, sequence, sample_count,
         sample_rate_hz, payload_bytes, capture_timestamp_us, adc_dc_mean,
         envelope_peak, alaw_reference, negative_ns, damp_ns, positive_ns,
         dropped_frames, payload_crc32) = values

        if magic != MAGIC:
            raise AssertionError("resynchronization failure")
        if version != PROTOCOL_VERSION:
            del self.buffer[0]
            raise ValueError(f"unsupported protocol version {version}")
        if payload_type not in PAYLOAD_NAMES:
            del self.buffer[0]
            raise ValueError(f"invalid payload type {payload_type}")
        if not (1 <= sample_count <= MAX_SAMPLE_COUNT):
            del self.buffer[0]
            raise ValueError(f"implausible sample count {sample_count}")
        expected = expected_payload_bytes(payload_type, sample_count)
        if payload_bytes != expected:
            del self.buffer[0]
            raise ValueError(
                f"{PAYLOAD_NAMES[payload_type]} payload size {payload_bytes} "
                f"!= expected {expected} for {sample_count} samples")

        frame_size = HEADER.size + payload_bytes
        self._fill(frame_size)
        payload = bytes(self.buffer[HEADER.size:frame_size])
        del self.buffer[:frame_size]
        actual_crc = zlib.crc32(payload) & 0xFFFFFFFF
        if actual_crc != payload_crc32:
            raise ValueError(
                f"CRC mismatch: header={payload_crc32:08x} "
                f"actual={actual_crc:08x}")

        header = FrameHeader(
            version=version, payload_type=payload_type, flags=flags,
            sequence=sequence, sample_count=sample_count,
            sample_rate_hz=sample_rate_hz, payload_bytes=payload_bytes,
            capture_timestamp_us=capture_timestamp_us, adc_dc_mean=adc_dc_mean,
            envelope_peak=envelope_peak, alaw_reference=alaw_reference,
            negative_ns=negative_ns, damp_ns=damp_ns, positive_ns=positive_ns,
            dropped_frames=dropped_frames, payload_crc32=payload_crc32)
        return Frame(header, payload)


def alaw_decode(encoded, reference: float) -> np.ndarray:
    """Expand positive-envelope A-law bytes back into ADC-count amplitude."""
    y = np.asarray(encoded, dtype=np.float64) / 255.0
    denominator = 1.0 + np.log(A_LAW_A)
    crossover = 1.0 / denominator
    x = np.where(
        y < crossover,
        y * denominator / A_LAW_A,
        np.exp(y * denominator - 1.0) / A_LAW_A,
    )
    return (reference * x).astype(np.float32)


def parse_status(line: str) -> dict:
    """Parse a firmware ``status`` line into a typed dict (leading ``OK``
    optional). See docs/ understanding_figures for field meanings."""
    text = line.strip()
    if text.startswith("OK "):
        text = text[3:].strip()
    if text.startswith("ERR"):
        raise ValueError(f"not a status line (error line): {line!r}")

    fields = {}
    for token in text.split():
        key, sep, value = token.partition("=")
        if sep:
            fields[key] = value
    if "board" not in fields or "firmware" not in fields:
        raise ValueError(f"does not look like a status line: {line!r}")

    out = {}
    int_keys = ("samples", "sample_rate", "dac", "drops", "dsp_us", "worst_us",
                "envelope_max_rate", "alaw_max_rate")
    for key, value in fields.items():
        if key in int_keys:
            out[key] = int(value)
        elif key == "scale":
            out[key] = float(value)
        elif key == "pulser":
            out["pulser"] = value
            out["pulser_armed"] = value == "armed"
        elif key == "pulse":
            parts = value.split("/")
            out["pulse"] = ({"negative_ns": int(parts[0]),
                             "damp_ns": int(parts[1]),
                             "positive_ns": int(parts[2]),
                             "order": parts[3]}
                            if len(parts) == 4 else value)
        elif key == "stream":
            parts = value.split("/")
            out["stream"] = ({"state": parts[0], "active": parts[0] == "on",
                              "rate_hz": int(parts[1])}
                             if len(parts) == 2 else value)
        elif key == "stages_us":
            parts = value.split("/")
            out["stages_us"] = {name: int(part)
                                for name, part in zip(STATUS_STAGE_NAMES, parts)}
        else:
            out[key] = value
    return out


def describe_status(status) -> str:
    """Human-readable multi-line summary of a status line or parsed dict,
    including the per-stage DSP microsecond times."""
    s = parse_status(status) if isinstance(status, str) else dict(status)
    rows = ["pic0rick status"]

    def add(label, value):
        rows.append(f"  {label:<16} {value}")

    add("board/package", f"{s.get('board', '?')} / {s.get('package', '?')}")
    add("firmware", s.get("firmware", "?"))
    add("dsp backend", s.get("dsp_backend", "?"))
    add("cmsis-dsp", s.get("cmsis", "?"))
    add("acquisition",
        f"{s.get('samples', '?')} samples @ {s.get('sample_rate', 0) / 1e6:g} MS/s")
    add("pulser", s.get("pulser", "?"))
    pulse = s.get("pulse")
    if isinstance(pulse, dict):
        add("pulse (ns)", f"neg {pulse['negative_ns']} / damp {pulse['damp_ns']} "
                          f"/ pos {pulse['positive_ns']}, {pulse['order']}")
    add("dac / scale", f"{s.get('dac', '?')} / {s.get('scale', '?')}")
    stream = s.get("stream")
    if isinstance(stream, dict):
        add("stream", f"{stream['state']} @ {stream['rate_hz']} Hz")
    add("dropped frames", s.get("drops", "?"))
    stages = s.get("stages_us")
    if isinstance(stages, dict):
        rows.append("  DSP stage times (us):")
        for k in STATUS_STAGE_NAMES:
            rows.append(f"      {k:<12} {stages.get(k, '?')}")
    add("dsp_us / worst",
        f"{s.get('dsp_us', '?')} / {s.get('worst_us', '?')} "
        f"({s.get('performance', '?')})")
    add("max rates (Hz)",
        f"envelope {s.get('envelope_max_rate', '?')}, "
        f"alaw {s.get('alaw_max_rate', '?')}")
    return "\n".join(rows)
