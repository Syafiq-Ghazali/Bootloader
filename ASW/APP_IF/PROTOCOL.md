# CAN application interface (protocol version 2)

`APP_IF.c` handles commands by calling the existing `BL_img` and `BL_clstr`
services. `BL_if` packs and unpacks CAN messages. `BL_sm` owns the boot lifecycle;
there is no second state machine to start.

This interface accesses the MCU's application flash. The external SPI NAND is a
separate project and is not addressed by these commands.

## Startup and host connection

The entry point initializes the board, flash API, CAN, and timer, then starts
`BL_smStart()`. The bootloader validates flash and sends unsolicited version and
system-status replies with tag zero. It then waits in `MENU` for CAN requests.

- A valid application runs after 8 seconds without requests.
- An invalid or erased application stays in `MENU` waiting for an update.
- Send `KEEP_ALIVE` periodically to restart the wait period. During an update,
  send data or keep-alives less than 1 second apart; 250 ms is a useful interval.
- An update timeout discards the pending RAM buffer, rechecks the saved image,
  reports the timeout, and returns to `MENU`. Flash already programmed remains.
- Errors return to `MENU` after reporting status, allowing another update.
- Successful `UPDATE_FINISH` resets the device; the normal startup wait follows.

CAN has no connection handshake. The Python client opens its CAN adapter, sends
`GET_INFO`, and waits for replies to confirm that the bootloader is reachable.
Once the application is running, it must arrange a reset into the bootloader if
updates are to be initiated without physically resetting the board.

## Bus and framing

Current board settings: classic CAN, standard 11-bit identifiers, 500 kbit/s,
GPIO 4 TX and GPIO 5 RX. A CAN transceiver and CAN adapter are required.
Board-specific settings are in `BRD/BRD_CAN.h`.

| CAN ID | Direction | Contents |
|---|---|---|
| `0x201` | Host to bootloader | Command request, exactly 8 bytes |
| `0x200` | Bootloader to host | Reply, exactly 8 bytes |
| `0x203` | Host to bootloader | Sequenced write data, exactly 8 bytes |
| `0x202` | Host to bootloader | Older raw write data, 2/4/6/8 bytes |

All integers are little-endian. Addresses and lengths count **16-bit words**.
For example, advancing a target address by 1 moves to the next 16-bit word;
Python serializes that word as two bytes, low byte first.

Requests contain arguments in bytes 0-5, an 8-bit host-selected tag in byte 6,
and the command number in byte 7. Unused argument bytes should be zero. Replies
echo the tag in byte 6 and contain a reply type in byte 7. Only one host and one
outstanding command are supported. Match both the tag and command in status
replies; ignore unsolicited startup messages while awaiting a command response.

Use a different tag for each new command. A write block is an exception: its
`WRITE_BEGIN`, sequenced data, and `WRITE_COMMIT` must all use the same block tag.
Use a different tag for each new block or restarted block.

## Commands

The original command numbers 0-8 are retained. Public WRITE names below map to
the existing `LOAD_CONFIGURE`, `DATA`, and `BUFFER_FLASH` names in the C code.

| Number | Command | Arguments in bytes 0-5 | Allowed state |
|---|---|---|---|
| 0 | `ERASE` | None; erase the entire application | MENU |
| 1 | `UPDATE_BEGIN` | `u32 imageSizeWords`, `u16 appVersion` | MENU |
| 2 | `WRITE_BEGIN` | `u32 address`, `u16 lengthWords` | UPDATE |
| 3 | `WRITE_COMMIT` | `u32 blockCrc`, two reserved bytes | UPDATE |
| 4 | `UPDATE_FINISH` | `u32 fullApplicationCrc`, two reserved bytes | UPDATE |
| 5 | `APP_RUN` | None | MENU |
| 6 | `RESET` | None | MENU, UPDATE |
| 7 | `GET_INFO` | None | MENU, UPDATE |
| 8 | `WRITE_DATA` acknowledgement selector | Data uses a separate CAN ID | UPDATE |
| 9 | `GET_STATUS` | None | MENU, UPDATE |
| 10 | `GET_MEMORY_MAP` | None | MENU, UPDATE |
| 11 | `KEEP_ALIVE` | None | MENU, UPDATE |
| 12 | `READ` | `u32 address`, `u16 lengthWords` | MENU, UPDATE |
| 13 | `ERASE_RANGE` | `u32 address`, `u16 lengthWords` | MENU |
| 14 | `VERIFY` | `u32 address`, `u16 lengthWords` | MENU, UPDATE |
| 15 | `UPDATE_ABORT` | None | UPDATE |

`UPDATE_BEGIN` records the pending version and image size; it does not erase flash.
The image size must be nonzero and fit in the application region. It describes
the image's span from the application start, including any gaps in that span.

`READ` returns up to 128 words per request. Split larger reads into multiple
requests. `ERASE_RANGE` requires a sector-aligned address and a nonzero length
that is a multiple of 4096 words. Use command 0 to erase the whole region.

`VERIFY` returns the calculated CRC, which the host compares with its expected
value. Address zero and length zero select the entire application region.
Other requests require a valid nonzero length; split ranges exceeding 65535 words
or use the whole-application selection.

`UPDATE_FINISH` rejects an uncommitted block. It compares the full flash CRC with
the host's expected CRC before erasing/writing metadata. Metadata is written from
an aligned, erased-fill buffer. A mismatch leaves the existing metadata untouched.

`UPDATE_ABORT` discards the RAM buffer and reloads the saved metadata. It does
not undo writes or erases. `APP_RUN` revalidates flash against saved metadata
before acknowledging and jumping to the application.

## Write transfer

Application addresses span `0x85000` through `0xAFFFF`, inclusive: `0x2B000` words.
Bootloader sectors occupy `0x80000`-`0x82FFF`; metadata occupies sectors at
`0x83000` and `0x84000`. Commands cannot read, erase, or program those sectors.
The linker also confines bootloader code to its reserved region.

Write blocks start at an 8-word boundary, have a length divisible by 8, and
contain at most 128 words. Pad the last block with erased `0xFFFF` words. Erase
the relevant sectors before programming; avoid programming the same 8-word unit
twice without erasing. The firmware programs only the block's touched units.

After `WRITE_BEGIN`, send data frames on `0x203`:

| Bytes | Value |
|---|---|
| 0-1 | `u16 offsetWords` relative to this block |
| 2-3 | First data word |
| 4-5 | Second data word, ignored if count is 1 |
| 6 | Block tag |
| 7 | Word count: 1 or 2 |

Wait for a `WRITE_DATA` acknowledgement before sending the next frame. The reply
includes the next expected word offset. Incorrect offsets/tags receive `RETRY`
without changing the buffer. An exact duplicate of the most recently accepted
data frame is acknowledged without appending twice.

Once all words are loaded, send `WRITE_COMMIT` with the block CRC. Repeating the
last successful commit with the same block tag and CRC acknowledges success
without programming flash again. Repeating an identical `WRITE_BEGIN` preserves
already loaded data; restart a damaged block with a new tag. Three CRC retries
are permitted; the fourth mismatch fails the update. A successful block resets
that counter.

Raw `0x202` frames append up to four words and retain the older transfer format.
They have no sequence/tag fields and cannot detect duplicate data frames; new
hosts should use `0x203`. Extra data beyond the configured block is rejected.

## Replies

All replies use byte 6 for the tag and byte 7 for the reply type.

| Type | Name | Bytes 0-5 |
|---|---|---|
| 0 | Request status | `u8 status`, `u8 command`, `u8 error`, reserved, `u16 nextWordOffset` |
| 1 | Versions | `u16 bootloaderVersion`, `u16 appVersion`, `u16 hardwareVersion` |
| 2 | System status | `u32 calculatedApplicationCrc`, `u8 appValid`, `u8 lastError` |
| 3 | Capabilities | `u16 protocolVersion`, `u16 hardwareSerialNumber`, `u16 sequencedDataWords` |
| 4 | State | `u8 state`, `u8 blockReady`, `u32 wordsWrittenThisUpdate` |
| 5 | Memory start | `u32 applicationStart`, `u16 sectorWords` |
| 6 | Memory size | `u32 applicationWords`, `u16 maximumBlockWords` |
| 7 | Read data | `u16 offsetWords`, two `u16` data words |
| 8 | CRC result | `u32 calculatedCrc`, two reserved bytes |

An odd-length read pads the unused second word of its final reply with `0xFFFF`;
consume only the requested number of words. Read offsets are relative to the
request, making missing data replies detectable.

`GET_INFO` returns types 1, 3, and 2, followed by request completion. `GET_STATUS`
returns types 4 and 2, followed by completion. `GET_MEMORY_MAP` returns types 5
and 6, followed by completion. Reads and verification return progress, their
data/result replies, then completion. Long flash operations also report progress.

Status values: 0 `IN_PROGRESS`, 1 `COMPLETE`, 2 `FAILED`, 3 `RETRY`, 4 `INVALID`.
State values: 0 startup, 1 menu, 2 update, 3 application run, 4 reset, 5 error.

| Error | Meaning |
|---|---|
| 0 | None |
| 1 | Update timeout |
| 2 | Flash operation failed |
| 3 | Block CRC mismatch |
| 4 | Legacy data error |
| 5 | Block configuration failed |
| 6 | Invalid application size |
| 7 | Application CRC mismatch / invalid saved image |
| 8 | Address outside application flash |
| 9 | Incorrect erase/write alignment |
| 10 | Command unavailable in this state / no active block |
| 11 | Incorrect data offset or block tag |
| 12 | Incorrect length or incomplete block |
| 13 | CAN transmission timed out |

Request-status errors describe that request. System status retains the last
error for diagnosis. The application CRC in system status is refreshed at startup,
erase, finish, abort, and error recovery; use `VERIFY` for a fresh calculation
while an update is in progress.

## Python example and update sequence

The firmware does not include an adapter-specific Python client yet. A client
can generate the wire messages using the standard library:

```python
import struct
import zlib

def request(command, tag, arguments=b""):
    if len(arguments) > 6:
        raise ValueError("Command arguments exceed six bytes")
    return arguments.ljust(6, b"\0") + bytes([tag, command])

keep_alive = request(11, tag=1)  # send on CAN ID 0x201
read_eight_words = request(12, tag=2, arguments=struct.pack("<IH", 0x85000, 8))
write_begin = request(2, tag=3, arguments=struct.pack("<IH", 0x85000, 8))
first_two_words = struct.pack("<HHHBB", 0, 0x1234, 0xABCD, 3, 2)  # CAN ID 0x203
block_crc = zlib.crc32(struct.pack("<8H", *words))
```

CRC uses reflected CRC-32, polynomial `0xEDB88320`, initial register/final XOR
`0xFFFFFFFF`. Python's `zlib.crc32` matches it. Compute the complete application
CRC over `0x56000` serialized bytes, filling unused words and gaps with `0xFFFF`.

Typical sequence: reset board, `GET_INFO`, `GET_MEMORY_MAP`, `ERASE`,
`UPDATE_BEGIN`, then repeat `WRITE_BEGIN` → data frames → `WRITE_COMMIT` for each
block. Optionally read back or verify, then send `UPDATE_FINISH`. The host can
present a single `flash_firmware()` method that performs this sequence.

Serialize commands and wait for their final reply. Allow a longer host timeout
for full erase and CRC operations, and send keep-alives between operations while
preparing more data. After an ambiguous timeout on erase/finish/reset, query the
device state rather than blindly repeating the operation.

## Validation

Run from the repository root:

```text
python tests/test_crc.py --cc C:/msys64/ucrt64/bin/gcc.exe
python tests/test_commands.py --cc C:/msys64/ucrt64/bin/gcc.exe
python tests/build_ti.py --compiler C:/ti/ccs2041/ccs/tools/compiler/ti-cgt-c2000_22.6.3.LTS --c2000ware C:/ti/C2000Ware_6_00_01_00
```

Flash configurations use optimization level 2, size preference 0, and function
subsections so the bootloader fits in its three reserved sectors. The target
check compiles and links without programming hardware. CAN signalling, actual
flash timing, and the application jump still require testing on the board.
