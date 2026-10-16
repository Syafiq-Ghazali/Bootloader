# Python PCAN client

A command-line tool and reusable Python API for this repository's CAN bootloader.
Defaults: `PCAN_USBBUS1`, classic CAN at **500 kbit/s**, protocol version 2.

## Install on Windows

Use Python 3.10 or newer. Install the driver for your PEAK adapter and make
PCAN-Basic available for your Python architecture. See the official
[python-can installation instructions](https://python-can.readthedocs.io/en/stable/installation.html#pcan)
and [PEAK PCAN-Basic downloads](https://www.peak-system.com/products/software/development-packages/pcan-basic/?L=1).
The adapter integration uses [python-can's PCAN backend](https://python-can.readthedocs.io/en/stable/interfaces/pcan.html).

From the **Bootloader repository root** in PowerShell:

```powershell
py -m venv host/.venv
.\host\.venv\Scripts\python.exe -m pip install -e ./host
.\host\.venv\Scripts\canboot.exe --help
```

The examples below assume the virtual environment is activated, or that
`canboot` is replaced with `.\host\.venv\Scripts\canboot.exe`.
`python -m canboot` is also supported.

## Connect and inspect

Connect the PCAN adapter to the board's CAN transceiver with the appropriate
bus wiring and termination. Reset the board and run a command during its
8-second bootloader window:

```text
canboot info
canboot status
canboot map
canboot --channel PCAN_USBBUS2 --bitrate 500000 info
canboot --device-id 0x1234 info
canboot hold
```

Opening the adapter does not reset the MCU. The client confirms communication
using `GET_INFO` and checks protocol compatibility before starting its session.
A valid application starts after the bootloader's inactivity timeout; an invalid
application stays in the bootloader. `hold` sends keep-alives until Ctrl+C.
Use one client process per board; do not run `hold` beside a flashing process.
Keep-alives stop when the client closes, starts the application, or resets the MCU.

## Flash an application

```text
canboot flash application.bin --version 1
```

`write` is an alias for the same complete operation:

```text
canboot write application.bin --version 1
```

The client checks the image and flash map, erases the whole application region,
begins an update, sends blocks with sequenced acknowledgements, verifies the full
flash CRC, and finalizes the metadata. The device then resets. Progress is printed
to stderr; the final result is JSON on stdout.

The file must be a **raw little-endian application binary** linked to start at
target word address `0x85000`. Two file bytes encode one target word. For example,
bytes `34 12` encode word `0x1234`. Use the application image, not the bootloader
image. `.out`/ELF and TI/Intel `.hex` files need to be exported to this format;
this version does not guess their address or byte-order conventions.

An odd final byte is paired with `0xFF`. The final write is padded to an 8-word
boundary, and unused flash/gaps are filled with `0xFFFF` for the application CRC.
Completely erased blocks need no transfer after the full erase. The application
entry word must be present. A failed transfer attempts to abort the update;
already programmed flash is not restored.

## Read, erase, verify, and control

Addresses and lengths below use **16-bit target words**, not file bytes.

```text
canboot read 0x85000 128 readback.bin
canboot erase
canboot erase --address 0x85000 --words 0x1000
canboot verify
canboot verify --address 0x85000 --words 128
canboot verify --expected 0x12345678
canboot abort
canboot run
canboot reset
```

Reads are split into requests of up to 128 words and saved as little-endian
binary. Range erases require sector alignment and can span multiple requests.
Range CRC requests support up to 65535 words; omit the range for the full
application CRC. `verify` reports the calculated CRC; `--expected` also compares
it and returns a failure on mismatch. `run` asks the device to validate and start
the application. `abort` only applies to an update still active on the target.

CLI exit codes: 0 success, 1 operation/input failure, 2 argument syntax error,
130 interrupted by Ctrl+C. `--verbose` logs CAN frames. Global options go before
the command. Default reply timeout is 0.2 seconds with two retries for repeatable
requests; erase, CRC, and finish use a 30-second timeout. Override with
`--timeout`, `--operation-timeout`, and `--retries`.

Only queries, block configuration, sequenced data, and block commits are retried
automatically. A lost erase/finish/reset/run acknowledgement has an ambiguous
outcome: check the device's state before repeating it. Data acknowledgements are
matched by both the block tag and next-word offset, so delayed acknowledgements
cannot silently skip data. A keep-alive failure is surfaced to subsequent API calls.

## Python API

```python
from canboot import BootloaderClient, FirmwareImage

with BootloaderClient.open_pcan(channel="PCAN_USBBUS1", bitrate=500000) as client:
    print(client.device)
    print(client.get_status())
    data = client.read(client.memory.start, 16)
    crc = client.crc()  # Entire application
```

Flash through the same API:

```python
with BootloaderClient.open_pcan() as client:
    image = FirmwareImage.from_file("application.bin", client.memory.start)
    client.flash_firmware(image, version=1,
                          progress=lambda done, total: print(done, total))
```

For custom update flows, use `erase()`, `begin_update(image_words, version)`,
`write(address, data)`, `verify(expected_crc)`, and `finish_update(full_crc)`.
`write` requires an active update and an aligned address; it pads the last
8-word unit with erased words. Keep the session open for the entire update.
`abort_update()` discards the pending buffer and returns to the menu.

All transport operations share one lock with the keep-alive worker. Keep-alives
run while the session is idle and do not interleave active request/reply pairs.
Progress callbacks run during flashing and should return promptly: the target's
update inactivity timeout is one second. There is no automatic update resume.
After finish/run/reset, close the client and reconnect with a new instance.

For sparse images constructed in Python, `FirmwareImage.segments` accepts
`(word_address, raw_bytes)` pairs; it checks overlap, fills gaps, and computes
CRC over the entire reported application region. The application entry segment
must still be included.

## Tests

```powershell
.\host\.venv\Scripts\python.exe -m unittest discover -s host/tests -v
```

With GCC installed (or `CC` pointing to it), the tests compile and drive the
production C firmware over its actual wire format. They exercise full updates,
CRC checks, lost/duplicate acknowledgements, missing read frames, timeouts,
keep-alives, and a python-can virtual bus. Without GCC, those integration tests
are skipped; image, transport, and CLI unit tests still run. The Windows test
runner also detects `C:/msys64/ucrt64/bin/gcc.exe`.

Physical PCAN communication, real flash timing, and application startup still
need validation on the board. Wire details are in the
[firmware protocol](../ASW/APP_IF/PROTOCOL.md).
