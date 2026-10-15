# Bootloader

CRC uses reflected CRC-32 with polynomial `0xEDB88320`, initial register
`0xFFFFFFFF`, and final XOR `0xFFFFFFFF` (compatible with Python `zlib.crc32`).
Pass `0` for the first CRC call and the previous returned CRC for subsequent calls.

`BCRC_compute` takes a count of 16-bit words and processes each word's low byte
before its high byte. `BCRC_wordAppend` appends one word; `BCRC_ffffAdd` appends a
count of erased `0xFFFF` words. All three functions accept a previously returned
CRC, and zero-length operations leave it unchanged. The erased-fill helper
previously processed only one byte per count; callers must now supply word counts.

Cluster CRC covers exactly the configured cluster length, excluding alignment
padding in the flash block buffer. Application CRC covers the entire word-addressed
flash range `0x85000` through `0xAFFFF` inclusive (`0x2B000` words). The host must
include erased `0xFFFF` words in gaps and after the last image segment, and exclude
the bootloader and metadata sectors. For example, on the host:

```python
import struct
import zlib

cluster_crc = zlib.crc32(struct.pack("<" + "H" * len(words), *words))
# application_bytes must contain the full application region, including 0xFF fill.
app_crc = zlib.crc32(application_bytes)
```

Each cluster CRC is checked before programming. At `UPDATE_FINISH`, the bootloader
reads back the full application region and compares its calculated CRC with the
host's expected CRC before erasing or writing variable metadata. A mismatch returns
`FAILED` with `BL_ERROR_APP_CRC` (error value 7); existing error values retain their
numbers. System-status replies report the calculated flash CRC. Startup validates
the calculated CRC against the saved metadata before allowing the application to run.

Run host regression tests with Python and GCC:

```text
python Bootloader/tests/test_crc.py --cc C:/msys64/ucrt64/bin/gcc.exe
```

These tests compile the production CRC and state machine with hardware services
stubbed, compare CRCs with `zlib`, and exercise startup, update completion, metadata
failures, cluster retries, and erase invalidation. Hardware flash/CAN operation
requires validation on the target.
