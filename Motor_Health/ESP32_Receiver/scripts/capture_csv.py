"""Capture the ESP32 receiver's CSV stream; stop with Ctrl+C."""

import argparse
import csv
from dataclasses import dataclass
from datetime import datetime, timezone
from decimal import Decimal, InvalidOperation
from pathlib import Path
import re
import sys


FIELDS = ["receive_time_ms", "voltage_V", "current_A", "rpm", "temperature_C"]
# Match the firmware's exact printf format, not arbitrary numeric log messages.
ROW = re.compile(rb"[0-9]+,-?[0-9]+\.[0-9]{2},-?[0-9]+\.[0-9]{3},-?[0-9]+\.[0-9],-?[0-9]+\.[0-9]")
MAX_LINE_BYTES = 256


class DeviceResetError(Exception):
    pass


@dataclass
class CaptureStats:
    rows: int = 0
    first_ms: int | None = None
    last_ms: int | None = None

    @property
    def duration_s(self):
        return 0.0 if self.first_ms is None else (self.last_ms - self.first_ms) / 1000


def prompt_metadata():
    while True:
        value = input("Commanded duty percent (0-100; recorded declaration only): ").strip()
        try:
            duty = Decimal(value)
            if duty.is_finite() and 0 <= duty <= 100:
                break
        except InvalidOperation:
            pass
        print("Enter a finite number between 0 and 100.")
    values = []
    for label in ("Test condition", "Run ID"):
        while True:
            value = input(f"{label}: ").strip()
            if value:
                values.append(value)
                break
            print(f"{label} must not be empty.")
    return (format(duty, "f"), *values)


def parse_row(line):
    line = line.removesuffix(b"\n").removesuffix(b"\r")
    if ROW.fullmatch(line):
        return line.decode("ascii").split(",")
    return None


def capture(stream, output, metadata, stats):
    writer = csv.writer(output)
    writer.writerow(["host_time_utc", *FIELDS, "declared_duty_percent",
                     "test_condition", "run_id", "elapsed_s", "interval_ms"])
    output.flush()
    pending = bytearray()
    # Opening the port may land in the middle of a row; synchronize first.
    discarding = True
    while True:
        # Timeouts may split a row. Retain bytes until a newline arrives.
        chunk = stream.read(1)
        if not chunk:
            continue
        if chunk == b"\n":
            if not discarding:
                row = parse_row(bytes(pending))
                if row is not None:
                    receive_ms = int(row[0])
                    if stats.last_ms is not None and receive_ms < stats.last_ms:
                        raise DeviceResetError(
                            f"ESP32 timestamp reset: {stats.last_ms} -> {receive_ms} ms. "
                            "Stopped before the new-session row; start a new recording."
                        )
                    first_ms = receive_ms if stats.first_ms is None else stats.first_ms
                    elapsed_ms = receive_ms - first_ms
                    elapsed_s = f"{elapsed_ms // 1000}.{elapsed_ms % 1000:03d}"
                    interval_ms = "" if stats.last_ms is None else receive_ms - stats.last_ms
                    timestamp = datetime.now(timezone.utc).isoformat(timespec="milliseconds")
                    writer.writerow([timestamp, *row, *metadata, elapsed_s, interval_ms])
                    stats.first_ms = first_ms
                    stats.last_ms = receive_ms
                    stats.rows += 1
                    output.flush()
            pending.clear()
            discarding = False
        elif not discarding:
            pending.extend(chunk)
            if len(pending) > MAX_LINE_BYTES:
                pending.clear()
                discarding = True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="Serial port, e.g. COM6 or /dev/ttyUSB0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--output-dir", type=Path, default=Path("captures"))
    args = parser.parse_args()
    if args.baud <= 0:
        parser.error("--baud must be positive")
    try:
        import serial
    except ImportError:
        parser.exit(1, "Cannot import serial. Use the working interpreter via start_capture.cmd.\n")

    path = None
    stats = CaptureStats()
    try:
        metadata = prompt_metadata()
        args.output_dir.mkdir(parents=True, exist_ok=True)
        stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S_%fZ")
        destination = (args.output_dir / f"can_{stamp}.csv").resolve()
        # Set control lines before opening to avoid intentionally resetting ESP32.
        with serial.Serial(port=None, baudrate=args.baud, timeout=1) as stream:
            stream.dtr = False
            stream.rts = False
            stream.port = args.port
            stream.open()
            with destination.open("x", newline="", encoding="utf-8") as output:
                path = destination
                print(f"Saving to {path} (Ctrl+C to stop)", flush=True)
                capture(stream, output, metadata, stats)
    except KeyboardInterrupt:
        print("\nCapture stopped.")
        return 0
    except EOFError:
        print("Input closed; recording not started.", file=sys.stderr)
        return 1
    except (OSError, serial.SerialException, DeviceResetError) as exc:
        print(f"Capture failed: {exc}", file=sys.stderr)
        return 1
    finally:
        if path is not None:
            print(f"File closed: {path}")
            print(f"Data rows saved: {stats.rows}")
            print(f"Recorded duration (ESP32 first-to-last reading): {stats.duration_s:.3f} s")
            if stats.rows == 0:
                print("No valid CSV data rows received.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
