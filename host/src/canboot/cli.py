"""Command-line entry point for PCAN bootloader operations."""
import argparse
from dataclasses import asdict
import json
import logging
from pathlib import Path
import sys

from .client import BootloaderClient
from .errors import BootloaderError
from .image import FirmwareImage


def number(value):
    try:
        return int(value, 0)
    except ValueError as exc:
        raise argparse.ArgumentTypeError("Use a decimal integer or a 0x-prefixed hexadecimal integer") from exc


def parser():
    result = argparse.ArgumentParser(prog="canboot", description="CAN bootloader client for a PEAK PCAN adapter")
    result.add_argument("--channel", default="PCAN_USBBUS1", help="PCAN channel (default: PCAN_USBBUS1)")
    result.add_argument("--bitrate", type=number, default=500000, help="Classic CAN bitrate (default: 500000)")
    result.add_argument("--device-id", type=number, help="Optional PCAN hardware device ID")
    result.add_argument("--timeout", type=float, default=0.2, help="Reply timeout in seconds (default: 0.2)")
    result.add_argument("--operation-timeout", type=float, default=30.0, help="Erase/CRC/finish timeout (default: 30)")
    result.add_argument("--retries", type=int, default=2, help="Retries for repeatable requests (default: 2)")
    result.add_argument("--verbose", action="store_true", help="Log transmitted and received CAN frames")
    commands = result.add_subparsers(dest="command", required=True)
    for name, help_text in (("info", "Read device versions and protocol capabilities"),
                            ("status", "Read bootloader state and update progress"),
                            ("map", "Read application flash layout"),
                            ("run", "Start the validated application"),
                            ("reset", "Reset the target MCU"),
                            ("abort", "Abort an active update; programmed flash remains")):
        commands.add_parser(name, help=help_text)
    hold = commands.add_parser("hold", help="Keep the bootloader waiting until Ctrl+C")
    hold.add_argument("--seconds", type=float, help="Optional duration instead of waiting indefinitely")
    read = commands.add_parser("read", help="Read flash into a little-endian binary file")
    read.add_argument("address", type=number, help="Target word address")
    read.add_argument("words", type=number, help="Number of 16-bit words")
    read.add_argument("output", type=Path)
    for name in ("erase", "verify"):
        command = commands.add_parser(name, help=f"{name.capitalize()} the entire application or a word range")
        command.add_argument("--address", type=number, help="Target word address")
        command.add_argument("--words", type=number, help="Number of 16-bit words")
        if name == "verify":
            command.add_argument("--expected", type=number, help="Expected CRC-32; a mismatch fails the command")
    flash = commands.add_parser("flash", aliases=["write"], help="Erase, write, verify, and finalize a .bin firmware image")
    flash.add_argument("file", type=Path, help="Raw little-endian application .bin starting at the application entry")
    flash.add_argument("--version", type=number, required=True, help="Application version, 0-65535")
    return result


def display(value):
    print(json.dumps(value, indent=2))


def main(argv=None):
    arguments = parser().parse_args(argv)
    logging.basicConfig(level=logging.DEBUG if arguments.verbose else logging.WARNING,
                        format="%(levelname)s: %(message)s")
    try:
        if arguments.command in ("erase", "verify"):
            if (arguments.address is None) != (arguments.words is None):
                raise ValueError("Supply both --address and --words, or neither for the entire application")
        binary = None
        if arguments.command in ("flash", "write"):
            if arguments.file.suffix.lower() != ".bin":
                raise ValueError("Export a raw little-endian .bin first; .out and .hex files are not parsed")
            binary = arguments.file.read_bytes()
            if not 0 <= arguments.version <= 65535:
                raise ValueError("Application version must be 0-65535")
            # Reject malformed binaries before opening the adapter.
            FirmwareImage.from_binary(binary, 0)
        with BootloaderClient.open_pcan(channel=arguments.channel, bitrate=arguments.bitrate,
                                        device_id=arguments.device_id, timeout=arguments.timeout,
                                        operation_timeout=arguments.operation_timeout,
                                        retries=arguments.retries) as client:
            command = arguments.command
            if command == "info":
                data = asdict(client.device)
                data["app_crc"] = f"0x{client.device.app_crc:08X}"
                display(data)
            elif command == "status":
                status = client.get_status()
                data = asdict(status)
                data["state_name"] = status.state.name
                data["app_crc"] = f"0x{status.app_crc:08X}"
                display(data)
            elif command == "map":
                data = asdict(client.memory)
                data["start"] = f"0x{client.memory.start:X}"
                data["end"] = f"0x{client.memory.start + client.memory.words - 1:X}"
                display(data)
            elif command == "read":
                data = client.read(arguments.address, arguments.words)
                arguments.output.write_bytes(data)
                display({"file": str(arguments.output), "words": len(data) // 2})
            elif command == "erase":
                client.erase(arguments.address, arguments.words)
                display({"status": "erased"})
            elif command == "verify":
                if arguments.expected is None:
                    crc = client.crc(arguments.address, arguments.words)
                else:
                    crc = client.verify(arguments.expected, arguments.address, arguments.words)
                display({"crc": f"0x{crc:08X}", "matches": True if arguments.expected is not None else None})
            elif command in ("flash", "write"):
                image = FirmwareImage.from_binary(binary, client.memory.start)

                def progress(done, total):
                    print(f"\rWriting {done}/{total} words ({done * 100 // total:3d}%)", end="", file=sys.stderr, flush=True)

                try:
                    crc = client.flash_firmware(image, arguments.version, progress)
                finally:
                    print(file=sys.stderr)
                display({"status": "flashed; device resetting", "crc": f"0x{crc:08X}"})
            elif command == "hold":
                print("Holding bootloader; press Ctrl+C to release.", file=sys.stderr)
                client.hold(arguments.seconds)
            elif command == "abort":
                client.abort_update()
                display({"status": "update aborted"})
            elif command == "run":
                client.run()
                display({"status": "application starting"})
            elif command == "reset":
                client.reset()
                display({"status": "device resetting"})
        return 0
    except KeyboardInterrupt:
        print("Interrupted.", file=sys.stderr)
        return 130
    except (BootloaderError, OSError, ValueError) as exc:
        print(f"canboot: {exc}", file=sys.stderr)
        return 1
