#!/usr/bin/env python3
"""Capture and validate pic0rick USB CDC binary frames."""

from __future__ import annotations

import argparse
import dataclasses
import json
import struct
import sys
import time
import zlib
from pathlib import Path
from typing import BinaryIO, Iterable

import numpy as np

MAGIC = b"P0RK"
PROTOCOL_VERSION = 1
SAMPLE_COUNT = 4096
HEADER = struct.Struct("<4sBBHIIIIQfffIIIII")
PAYLOAD_NAMES = {1: "raw", 2: "envelope", 3: "alaw"}
PAYLOAD_BYTES = {1: SAMPLE_COUNT * 2, 2: SAMPLE_COUNT * 4, 3: SAMPLE_COUNT}
A_LAW_A = 87.6
EXPECTED_FIRMWARE = "1.6"
FLAG_SELFTEST = 1 << 3
SELFTEST_CASE_SHIFT = 8
SELFTEST_NAMES = (
    "zero",
    "dc",
    "sinusoid",
    "am",
    "two-bursts",
    "impulse",
    "clipping",
)
# Fixed order of the per-stage timings in the status line's `stages_us` field,
# matching u4rk_dsp_envelope() in firmware/pic0rick/dsp.c.
STATUS_STAGE_NAMES = (
    "preprocess",
    "forward_fft",
    "mask",
    "inverse_fft",
    "magnitude",
    "alaw",
)


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
        return PAYLOAD_NAMES[self.payload_type]

    @property
    def selftest_case(self) -> int | None:
        if not (self.flags & FLAG_SELFTEST):
            return None
        return (self.flags >> SELFTEST_CASE_SHIFT) & 0xFF


@dataclasses.dataclass(frozen=True)
class Frame:
    header: FrameHeader
    payload: bytes

    def samples(self) -> np.ndarray:
        if self.header.payload_type == 1:
            return np.frombuffer(self.payload, dtype="<u2").copy()
        if self.header.payload_type == 2:
            return np.frombuffer(self.payload, dtype="<f4").copy()
        return np.frombuffer(self.payload, dtype=np.uint8).copy()


class FrameReader:
    """Buffered reader that discards text/noise until the next P0RK magic."""

    def __init__(self, stream: BinaryIO, read_size: int = 4096):
        self.stream = stream
        self.read_size = read_size
        self.buffer = bytearray()

    def _fill(self, needed: int) -> None:
        while len(self.buffer) < needed:
            missing = needed - len(self.buffer)
            # Do not request a full 4096-byte block when only the final header
            # or payload tail is missing; PySerial waits for the requested byte
            # count and previously caused the apparent 20/21-frame pause.
            available = int(getattr(self.stream, "in_waiting", 0) or 0)
            request_size = min(self.read_size, max(missing, available))
            chunk = self.stream.read(request_size)
            if not chunk:
                raise TimeoutError(f"timed out with {len(self.buffer)}/{needed} bytes")
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
        (
            magic,
            version,
            payload_type,
            flags,
            sequence,
            sample_count,
            sample_rate_hz,
            payload_bytes,
            capture_timestamp_us,
            adc_dc_mean,
            envelope_peak,
            alaw_reference,
            negative_ns,
            damp_ns,
            positive_ns,
            dropped_frames,
            payload_crc32,
        ) = values

        if magic != MAGIC:
            raise AssertionError("resynchronization failure")
        if version != PROTOCOL_VERSION:
            del self.buffer[0]
            raise ValueError(f"unsupported protocol version {version}")
        if payload_type not in PAYLOAD_BYTES:
            del self.buffer[0]
            raise ValueError(f"invalid payload type {payload_type}")
        if sample_count != SAMPLE_COUNT:
            del self.buffer[0]
            raise ValueError(f"invalid sample count {sample_count}")
        if payload_bytes != PAYLOAD_BYTES[payload_type]:
            del self.buffer[0]
            raise ValueError(
                f"invalid {PAYLOAD_NAMES[payload_type]} payload size {payload_bytes}"
            )

        frame_size = HEADER.size + payload_bytes
        self._fill(frame_size)
        payload = bytes(self.buffer[HEADER.size:frame_size])
        del self.buffer[:frame_size]
        actual_crc = zlib.crc32(payload) & 0xFFFFFFFF
        if actual_crc != payload_crc32:
            raise ValueError(
                f"CRC mismatch: header={payload_crc32:08x} actual={actual_crc:08x}"
            )

        header = FrameHeader(
            version=version,
            payload_type=payload_type,
            flags=flags,
            sequence=sequence,
            sample_count=sample_count,
            sample_rate_hz=sample_rate_hz,
            payload_bytes=payload_bytes,
            capture_timestamp_us=capture_timestamp_us,
            adc_dc_mean=adc_dc_mean,
            envelope_peak=envelope_peak,
            alaw_reference=alaw_reference,
            negative_ns=negative_ns,
            damp_ns=damp_ns,
            positive_ns=positive_ns,
            dropped_frames=dropped_frames,
            payload_crc32=payload_crc32,
        )
        return Frame(header, payload)


def alaw_encode(envelope: np.ndarray, reference: float) -> np.ndarray:
    """Positive-envelope A-law reference encoder (A=87.6)."""
    if not np.isfinite(reference) or reference <= 0:
        raise ValueError("reference must be finite and positive")
    x = np.clip(np.asarray(envelope, dtype=np.float64) / reference, 0.0, 1.0)
    denominator = 1.0 + np.log(A_LAW_A)
    y = np.where(
        x < 1.0 / A_LAW_A,
        A_LAW_A * x / denominator,
        (1.0 + np.log(np.maximum(A_LAW_A * x, np.finfo(float).tiny)))
        / denominator,
    )
    return np.floor(y * 255.0 + 0.5).astype(np.uint8)


def alaw_decode(encoded: np.ndarray, reference: float) -> np.ndarray:
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
    """Parse a firmware ``status`` line into a typed dict.

    Accepts the raw ``OK board=...`` string (the leading ``OK`` is optional).
    Scalar fields become ``int``/``float``/``str``; the compound fields
    ``pulse``, ``stream`` and ``stages_us`` become nested dicts. Unknown keys
    are preserved verbatim as strings so future firmware fields still surface.

    See ``understanding_figures.md`` for the meaning of every field.
    """
    text = line.strip()
    if text.startswith("OK "):
        text = text[3:].strip()
    if text.startswith("ERR"):
        raise ValueError(f"not a status line (got an error line): {line!r}")

    fields: dict[str, str] = {}
    for token in text.split():
        key, sep, value = token.partition("=")
        if sep:
            fields[key] = value

    if "board" not in fields or "firmware" not in fields:
        raise ValueError(f"does not look like a status line: {line!r}")

    out: dict = {}
    int_keys = (
        "samples", "sample_rate", "dac", "drops", "dsp_us", "worst_us",
        "envelope_max_rate", "alaw_max_rate",
    )
    compound = {"pulse", "stream", "stages_us"}

    for key, value in fields.items():
        if key in int_keys:
            out[key] = int(value)
        elif key == "scale":
            out[key] = float(value)
        elif key == "pulser":
            out["pulser"] = value                    # "armed" | "disarmed"
            out["pulser_armed"] = value == "armed"
        elif key == "pulse":
            parts = value.split("/")
            out["pulse"] = (
                {
                    "negative_ns": int(parts[0]),
                    "damp_ns": int(parts[1]),
                    "positive_ns": int(parts[2]),
                    "order": parts[3],
                }
                if len(parts) == 4
                else value
            )
        elif key == "stream":
            parts = value.split("/")
            out["stream"] = (
                {
                    "state": parts[0],               # "on" | "off"
                    "active": parts[0] == "on",
                    "rate_hz": int(parts[1]),
                }
                if len(parts) == 2
                else value
            )
        elif key == "stages_us":
            parts = value.split("/")
            out["stages_us"] = {
                name: int(part)
                for name, part in zip(STATUS_STAGE_NAMES, parts)
            }
        else:
            out[key] = value                         # board, package, ... verbatim

    for key in compound:
        out.setdefault(key, None)
    return out


def describe_status(line: str) -> str:
    """Return a human-readable, multi-line summary of a ``status`` line."""
    s = parse_status(line)
    rows: list[str] = ["pic0rick firmware status"]

    def add(label: str, value: object) -> None:
        rows.append(f"  {label:<20} {value}")

    add("board / package", f"{s.get('board', '?')} / {s.get('package', '?')}")
    add("firmware", s.get("firmware", "?"))
    add("DSP backend", s.get("dsp_backend", "?"))
    add("CMSIS-DSP", s.get("cmsis", "?"))
    rate = s.get("sample_rate", 0)
    add("acquisition", f"{s.get('samples', '?')} samples @ {rate / 1e6:g} MS/s")
    add("pulser", s.get("pulser", "?"))
    pulse = s.get("pulse")
    if isinstance(pulse, dict):
        add(
            "pulse timing",
            f"neg {pulse['negative_ns']} ns / damp {pulse['damp_ns']} ns / "
            f"pos {pulse['positive_ns']} ns, {pulse['order']}",
        )
    add("DAC last write", f"{s.get('dac', '?')} (0..1023 code)")
    add("A-law reference", f"{s.get('scale', '?')} ADC counts (full scale)")
    stream = s.get("stream")
    if isinstance(stream, dict):
        add("stream", f"{stream['state']}, {stream['rate_hz']} Hz requested")
    add("dropped frames", f"{s.get('drops', '?')} (cumulative since boot)")

    stages = s.get("stages_us")
    add("DSP last frame", f"{s.get('dsp_us', '?')} us total")
    if isinstance(stages, dict):
        labels = {
            "preprocess": "preprocess",
            "forward_fft": "forward FFT",
            "mask": "Hilbert mask",
            "inverse_fft": "inverse FFT",
            "magnitude": "magnitude",
            "alaw": "A-law encode",
        }
        for key, label in labels.items():
            rows.append(f"      {label:<16} {stages[key]} us")
    verdict = s.get("performance", "?")
    add("DSP worst case", f"{s.get('worst_us', '?')} us -> {verdict} (target <= 4500 us)")
    add(
        "max stream rates",
        f"envelope {s.get('envelope_max_rate', '?')} Hz, "
        f"A-law {s.get('alaw_max_rate', '?')} Hz",
    )
    return "\n".join(rows)


def check_sequences(frames: Iterable[Frame]) -> None:
    previous: int | None = None
    for frame in frames:
        if previous is not None and frame.header.sequence != (
            previous + 1
        ) & 0xFFFFFFFF:
            raise ValueError(
                f"sequence gap: got {frame.header.sequence}, expected "
                f"{(previous + 1) & 0xFFFFFFFF}"
            )
        previous = frame.header.sequence


def save_frames(
    frames: list[Frame], output: Path, final_status: str | None = None
) -> None:
    output.mkdir(parents=True, exist_ok=True)
    metadata: list[dict[str, object]] = []
    grouped: dict[int, list[Frame]] = {}
    for frame in frames:
        grouped.setdefault(frame.header.payload_type, []).append(frame)

    for payload_type, group in grouped.items():
        payload_name = PAYLOAD_NAMES[payload_type]
        samples = np.stack([frame.samples() for frame in group])
        np.save(output / f"{payload_name}.npy", samples, allow_pickle=False)
        if payload_type == 3 and len(group) <= 100:
            references = np.asarray(
                [frame.header.alaw_reference for frame in group],
                dtype=np.float32,
            )[:, None]
            np.save(
                output / "alaw_decoded.npy",
                np.stack(
                    [
                        alaw_decode(frame.samples(), frame.header.alaw_reference)
                        for frame in group
                    ]
                ).reshape(references.shape[0], SAMPLE_COUNT),
                allow_pickle=False,
            )
        for array_index, frame in enumerate(group):
            record = dataclasses.asdict(frame.header)
            record["payload_name"] = frame.header.payload_name
            record["array_index"] = array_index
            metadata.append(record)
    metadata.sort(key=lambda item: int(item["sequence"]))
    (output / "headers.json").write_text(
        json.dumps(metadata, indent=2), encoding="utf-8"
    )
    if final_status is not None:
        (output / "status.txt").write_text(final_status + "\n", encoding="utf-8")


def validate_selftest(frames: list[Frame]) -> None:
    try:
        from scipy.signal import hilbert
    except ImportError as exc:
        raise RuntimeError(
            "SciPy is required for self-test validation; install tools/requirements.txt"
        ) from exc

    if len(frames) != len(SELFTEST_NAMES) * 3:
        raise ValueError(f"expected 21 self-test frames, received {len(frames)}")

    grouped: dict[int, dict[int, Frame]] = {}
    for frame in frames:
        case = frame.header.selftest_case
        if case is None or case >= len(SELFTEST_NAMES):
            raise ValueError("frame is missing a valid self-test case flag")
        grouped.setdefault(case, {})[frame.header.payload_type] = frame

    failures: list[str] = []
    for case, case_name in enumerate(SELFTEST_NAMES):
        case_frames = grouped.get(case, {})
        if set(case_frames) != {1, 2, 3}:
            failures.append(f"{case_name}: missing raw/envelope/A-law frame")
            continue

        raw = case_frames[1].samples().astype(np.float32)
        firmware_envelope = case_frames[2].samples()
        reference = np.abs(hilbert(raw - np.mean(raw, dtype=np.float64)))
        scale = max(float(np.max(reference)), 1.0)
        normalized_rms = float(
            np.sqrt(np.mean((firmware_envelope - reference) ** 2)) / scale
        )
        firmware_peak = int(np.argmax(firmware_envelope))
        # Several deterministic vectors have mathematically equal maxima.
        # Accept any maximum tied at float32 precision, including circularly
        # adjacent samples, instead of depending on one np.argmax choice.
        peak_tolerance = max(scale * 1e-6, 1e-6)
        reference_peak_candidates = np.flatnonzero(
            reference >= float(np.max(reference)) - peak_tolerance
        )
        peak_distances = np.abs(reference_peak_candidates - firmware_peak)
        peak_distances = np.minimum(
            peak_distances, len(reference) - peak_distances
        )
        peak_delta = int(np.min(peak_distances))

        alaw_frame = case_frames[3]
        encoded_reference = alaw_encode(reference, alaw_frame.header.alaw_reference)
        alaw_error = int(
            np.max(
                np.abs(
                    alaw_frame.samples().astype(np.int16)
                    - encoded_reference.astype(np.int16)
                )
            )
        )
        print(
            f"{case_name:12s} nrms={normalized_rms:.3e} "
            f"peak_delta={peak_delta} "
            f"alaw_delta={alaw_error}"
        )
        if normalized_rms > 1e-4:
            failures.append(f"{case_name}: normalized RMS {normalized_rms:.3e}")
        if peak_delta > 1:
            failures.append(f"{case_name}: peak index differs by more than one")
        if alaw_error > 1:
            failures.append(f"{case_name}: A-law differs by {alaw_error} levels")

    if failures:
        raise AssertionError("; ".join(failures))


def read_response_line(port: BinaryIO) -> str:
    deadline = time.monotonic() + 3.0
    line = bytearray()
    while time.monotonic() < deadline:
        byte = port.read(1)
        if not byte:
            continue
        if byte == b"\n":
            text = line.decode("ascii", errors="replace").strip()
            if text.startswith(("OK", "ERR")):
                return text
            line.clear()
        elif byte != b"\r":
            line.extend(byte)
    raise TimeoutError("timed out waiting for command response")


def run(args: argparse.Namespace) -> int:
    try:
        import serial
    except ImportError as exc:
        raise RuntimeError(
            "pyserial is required; install tools/requirements.txt"
        ) from exc

    serial_options: dict[str, object] = {}
    if sys.platform != "win32":
        # Prevent ModemManager or a second terminal from sharing ttyACM while
        # a binary frame sequence is active.
        serial_options["exclusive"] = True
    port = serial.Serial(
        args.port, 115200, timeout=args.timeout, **serial_options
    )
    try:
        # Assert the conventional CDC terminal state and let Linux complete
        # its ACM control requests before sending the first command.
        port.dtr = True
        time.sleep(0.2)
        port.reset_input_buffer()
        port.reset_output_buffer()

        port.write(b"status\n")
        port.flush()
        status = read_response_line(port)
        print(status)
        if f"firmware={EXPECTED_FIRMWARE}" not in status:
            raise RuntimeError(
                f"expected firmware {EXPECTED_FIRMWARE}; flash the UF2 "
                "included with this tool"
            )

        if args.selftest:
            command = "dsp selftest"
            frame_count = len(SELFTEST_NAMES) * 3
            streaming = False
        elif args.rate:
            command = f"stream start {args.mode} {args.rate}"
            frame_count = args.frames
            streaming = True
        else:
            command = f"acq {args.mode}"
            frame_count = args.frames
            streaming = False

        port.write((command + "\n").encode("ascii"))
        port.flush()
        response = read_response_line(port)
        print(response)
        if response.startswith("ERR"):
            return 2

        reader = FrameReader(port)
        frames: list[Frame] = []
        for index in range(frame_count):
            frame = reader.read_frame()
            frames.append(frame)
            print(
                f"{index + 1}/{frame_count}: seq={frame.header.sequence} "
                f"type={frame.header.payload_name} flags=0x{frame.header.flags:04x} "
                f"drops={frame.header.dropped_frames} "
                f"peak={frame.header.envelope_peak:.5g}"
            )

        check_sequences(frames)
        final_status = None
        if streaming:
            port.write(b"stream stop\n")
            port.flush()
            # Drop any already-complete surplus frame and the stop ACK, then
            # obtain the firmware's accumulated timing/drop counters.
            time.sleep(0.25)
            port.reset_input_buffer()
            port.write(b"status\n")
            port.flush()
            final_status = read_response_line(port)
            print(final_status)

        save_frames(frames, args.output, final_status)
        if args.selftest:
            validate_selftest(frames)
        return 0
    finally:
        port.close()


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="CDC serial port, e.g. COM7")
    parser.add_argument(
        "--mode", choices=("raw", "envelope", "alaw"), default="alaw"
    )
    parser.add_argument(
        "--rate", type=int, default=0, help="stream rate; 0 requests one-shot"
    )
    parser.add_argument("--frames", type=int, default=1)
    parser.add_argument(
        "--timeout",
        type=float,
        default=30.0,
        help="per-read timeout in seconds (default: 30 for the self-test)",
    )
    parser.add_argument("--output", type=Path, default=Path("captures"))
    parser.add_argument(
        "--selftest",
        action="store_true",
        help="capture deterministic DSP vectors and compare with SciPy",
    )
    args = parser.parse_args(argv)
    if args.frames < 1:
        parser.error("--frames must be positive")
    if args.rate < 0:
        parser.error("--rate cannot be negative")
    if args.timeout <= 0:
        parser.error("--timeout must be positive")
    return args


if __name__ == "__main__":
    try:
        raise SystemExit(run(parse_args(sys.argv[1:])))
    except (AssertionError, RuntimeError, TimeoutError, ValueError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        raise SystemExit(1)
