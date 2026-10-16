"""Exercise real CAN framing, dispatch, state machine, cluster buffer, and CRC.

Only the CAN hardware, flash hardware/image access, and clock are simulated.
Run: python tests/test_commands.py --cc path/to/gcc
"""
import argparse
import ctypes
from contextlib import ExitStack, contextmanager
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[1]
BASE, SIZE = 0x85000, 0x2B000
HOST_TYPES = """
#include <stdbool.h>
#include <stdint.h>
#define _DATATYPE_H_
#define __interrupt
#define DINT ((void)0)
typedef bool Bool_t;
typedef uint16_t Uint16_t;
typedef uint32_t Uint32_t;
typedef float Float32_t;
#define TRUE ((Bool_t)1)
#define FALSE ((Bool_t)0)
"""
CAN_HEADER = """
#ifndef MOCK_BCAN_H
#define MOCK_BCAN_H
#include "BootloaderDefs.h"
typedef struct { Uint32_t id; Uint16_t buf[8]; Uint16_t bufLen; } CanMsg_t;
typedef struct { unsigned unused; } CanHandle_t;
typedef enum { CAN_STATUS_TX_OK, CAN_STATUS_TX_FULL, CAN_STATUS_RX_OK, CAN_STATUS_RX_EMPTY } CanStatus_t;
extern CanStatus_t BCAN_tx(CanHandle_t *, CanMsg_t *);
extern CanStatus_t BCAN_rx(CanHandle_t *, CanMsg_t *);
extern Bool_t BCAN_txPending(CanHandle_t *);
#endif
"""
HARNESS = r"""
#include <assert.h>
#include <string.h>
#include "APP_IF.h"
#include "BCRC.h"
#include "TMR.h"
#include "BRD_CAN.h"
CanHandle_t BRD_CAN_HANDLE[1];
static Uint16_t flash[BL_IMG_APPLICATION_REGION_SIZE_WORDS];
static ImgVariableMetadata_t metadata;
static CanMsg_t incoming, outgoing[256];
static Bool_t pending, txBusy;
static unsigned outCount, writeCount, eraseCount, metadataEraseCount, resetCount, startCount;
static Uint32_t now, lastWriteLength;
static Uint16_t metadataFailure;

CanStatus_t BCAN_tx(CanHandle_t *h, CanMsg_t *m)
{ (void)h; assert(outCount < 256); outgoing[outCount++] = *m; return CAN_STATUS_TX_OK; }
CanStatus_t BCAN_rx(CanHandle_t *h, CanMsg_t *m)
{ (void)h; if (!pending) return CAN_STATUS_RX_EMPTY; *m = incoming; pending = FALSE; return CAN_STATUS_RX_OK; }
Bool_t BCAN_txPending(CanHandle_t *h) { (void)h; return txBusy; }
void TMR_createAndStart(TmrHandle_t *t, Uint32_t d) { t->startTime = now; t->durationMs = d; }
Bool_t TMR_updateAndCheckExpiry(TmrHandle_t *t)
{ if (txBusy) ++now; return (now - t->startTime) >= t->durationMs; }
void SysCtl_resetDevice(void) { ++resetCount; }
void BL_imgAppStart(void) { ++startCount; }
void BL_imgVariableMetadataRead(ImgVariableMetadata_t *m) { *m = metadata; }
void BL_imgFixedMetadataRead(ImgFixedMetadata_t *m)
{ m->bootloaderVersion = 1; m->hardwareVersion = 2; m->hardwareSerialNumber = 42; }
Uint32_t BL_imgCrcCompute(void)
{ return BCRC_compute(flash, BL_IMG_APPLICATION_REGION_SIZE_WORDS, 0); }
Bool_t BL_imgRangeValid(Uint32_t a, Uint32_t n)
{ return n && a >= BL_IMG_APPLICATION_BEGIN_ADDR && a <= BFLASH_END && n <= BFLASH_END - a + 1; }
Uint16_t BL_imgRead(Uint32_t a, Uint16_t *d, Uint16_t n)
{ if (!BL_imgRangeValid(a, n)) return FAIL; memcpy(d, flash + a - BL_IMG_APPLICATION_BEGIN_ADDR, n * sizeof(*d)); return OK; }
Uint16_t BL_imgRangeCrcCompute(Uint32_t a, Uint32_t n, Uint32_t *crc)
{ if (!BL_imgRangeValid(a, n)) return FAIL; *crc = BCRC_compute(flash + a - BL_IMG_APPLICATION_BEGIN_ADDR, n, 0); return OK; }
Uint16_t BL_imgRangeErase(Uint32_t a, Uint32_t n)
{
    assert(BL_imgRangeValid(a, n)); assert(a % 0x1000 == 0 && n % 0x1000 == 0);
    ++eraseCount; memset(flash + a - BL_IMG_APPLICATION_BEGIN_ADDR, 0xFF, n * sizeof(*flash)); return OK;
}
Uint16_t BL_imgVariableMetadataErase(void)
{
    ++metadataEraseCount; if (metadataFailure == 1) return FAIL;
    memset(&metadata, 0xFF, sizeof(metadata)); return OK;
}
Uint16_t BL_imgVariableMetadataWrite(ImgVariableMetadata_t *m)
{ if (metadataFailure == 2) return FAIL; metadata = *m; return OK; }
Uint16_t BFLASH_bufferWrite(Uint32_t a, Uint16_t *d, Uint32_t n)
{
    assert(BL_imgRangeValid(a, n)); assert(a % 8 == 0 && n % 8 == 0);
    ++writeCount; lastWriteLength = n;
    for (Uint32_t i = 0; i < n; ++i)
    {
        if ((flash[a - BL_IMG_APPLICATION_BEGIN_ADDR + i] & d[i]) != d[i]) return FAIL;
        flash[a - BL_IMG_APPLICATION_BEGIN_ADDR + i] = d[i];
    }
    return OK;
}

void test_reset(unsigned valid)
{
    now = 0; pending = txBusy = FALSE;
    outCount = writeCount = eraseCount = metadataEraseCount = resetCount = startCount = 0;
    metadataFailure = 0; lastWriteLength = 0;
    memset(flash, 0xFF, sizeof(flash)); memset(&metadata, 0xFF, sizeof(metadata));
    if (valid)
    {
        for (unsigned i = 0; i < 8; ++i) flash[i] = (Uint16_t)(0x1000 + i);
        metadata.appVersion = 3; metadata.appSizeWords = 8; metadata.appCrc = BL_imgCrcCompute();
    }
    BL_ifInit(); BL_clstrInit(); BL_smInitialize(); BL_smExe(); BL_smExe(); outCount = 0;
}
void test_frame(Uint32_t id, const unsigned char *d, Uint16_t n)
{
    memset(&incoming, 0, sizeof(incoming)); incoming.id = id; incoming.bufLen = n;
    for (unsigned i = 0; i < n && i < 8; ++i) incoming.buf[i] = d[i];
    pending = TRUE; BL_smExe();
}
void test_clear(void) { outCount = 0; }
unsigned test_count(void) { return outCount; }
void test_reply(unsigned index, unsigned char *d)
{ assert(index < outCount); assert(outgoing[index].id == BL_IF_CAN_ID_REPLY); for (unsigned i = 0; i < 8; ++i) d[i] = (unsigned char)outgoing[index].buf[i]; }
void test_advance(Uint32_t ms) { now += ms; BL_smExe(); }
void test_step(void) { BL_smExe(); }
void test_tx_busy(unsigned b) { txBusy = b != 0; }
void test_metadata_failure(unsigned v) { metadataFailure = (Uint16_t)v; }
unsigned test_state(void) { return BL_smStateRetreive(); }
unsigned test_writes(void) { return writeCount; }
unsigned test_erases(void) { return eraseCount; }
unsigned test_metadata_erases(void) { return metadataEraseCount; }
unsigned test_starts(void) { return startCount; }
unsigned test_resets(void) { return resetCount; }
Uint32_t test_write_length(void) { return lastWriteLength; }
Uint32_t test_crc(void) { return BL_imgCrcCompute(); }
Uint32_t test_saved_crc(void) { return metadata.appCrc; }
"""


class Commands(unittest.TestCase):
    lib = None

    def setUp(self):
        self.lib.test_reset(0)

    def frames(self):
        result = []
        for index in range(self.lib.test_count()):
            payload = (ctypes.c_ubyte * 8)()
            self.lib.test_reply(index, payload)
            result.append(bytes(payload))
        return result

    def frame(self, can_id, payload):
        self.lib.test_clear()
        self.lib.test_frame(can_id, payload, len(payload))
        return self.frames()

    def command(self, command, payload=b"", tag=20):
        return self.frame(0x201, payload.ljust(6, b"\0") + bytes([tag, command]))

    def ack(self, frames):
        return [f for f in frames if f[7] == 0][-1]

    def begin(self):
        self.assertEqual(self.ack(self.command(1, struct.pack("<IH", 128, 9)))[0], 1)

    def block(self, tag=21, address=BASE, count=8):
        return self.command(2, struct.pack("<IH", address, count), tag)

    def data(self, offset, words, tag=21):
        padded = list(words) + [0] * (2 - len(words))
        return self.frame(0x203, struct.pack("<HHHBB", offset, *padded, tag, len(words)))

    def write_block(self, words=None, tag=21, address=BASE):
        words = list(range(8)) if words is None else words
        self.assertEqual(self.ack(self.block(tag, address, len(words)))[0], 1)
        for offset in range(0, len(words), 2):
            self.assertEqual(self.ack(self.data(offset, words[offset:offset + 2], tag))[0], 1)
        crc = zlib.crc32(struct.pack("<" + "H" * len(words), *words))
        self.assertEqual(self.ack(self.command(3, struct.pack("<I", crc), tag))[0], 1)
        return crc

    def advance(self, milliseconds):
        self.lib.test_step()
        self.lib.test_advance(milliseconds)

    def test_info_status_map_and_tags(self):
        frames = self.command(7, tag=73)
        self.assertEqual([f[7] for f in frames], [1, 3, 2, 0])
        self.assertTrue(all(f[6] == 73 for f in frames))
        self.assertEqual(struct.unpack("<HHH", frames[1][:6]), (2, 42, 2))
        frames = self.command(10)
        self.assertEqual(struct.unpack("<IH", frames[0][:6]), (BASE, 4096))
        self.assertEqual(struct.unpack("<IH", frames[1][:6]), (SIZE, 128))
        self.assertEqual([f[7] for f in self.command(9)], [4, 2, 0])

    def test_idle_without_valid_image_waits(self):
        self.advance(9000)
        self.advance(9000)
        self.assertEqual(self.lib.test_state(), 1)
        self.assertEqual(self.lib.test_starts(), 0)
        self.assertEqual(self.lib.test_resets(), 0)

    def test_valid_image_autoruns(self):
        self.lib.test_reset(1)
        self.advance(8000)
        self.assertEqual(self.lib.test_state(), 3)
        self.lib.test_step()
        self.assertEqual(self.lib.test_starts(), 1)

    def test_keep_alive_holds_menu(self):
        self.lib.test_reset(1)
        for _ in range(3):
            self.advance(7000)
            self.assertEqual(self.ack(self.command(11))[0], 1)
            self.assertEqual(self.lib.test_state(), 1)

    def test_update_timeout_and_abort(self):
        self.begin()
        self.block()
        self.advance(1000)
        self.assertEqual(self.lib.test_state(), 1)
        self.assertEqual([f for f in self.frames() if f[7] == 2][-1][5], 1)
        self.begin()
        self.block()
        self.assertEqual(self.ack(self.command(15))[0], 1)
        self.assertEqual(self.lib.test_state(), 1)
        self.assertEqual(self.lib.test_writes(), 0)

    def test_read_and_verify(self):
        self.lib.test_reset(1)
        frames = self.command(12, struct.pack("<IH", BASE, 3))
        data = [f for f in frames if f[7] == 7]
        self.assertEqual(struct.unpack("<HHH", data[0][:6]), (0, 0x1000, 0x1001))
        self.assertEqual(struct.unpack("<HHH", data[1][:6]), (2, 0x1002, 0xFFFF))
        frames = self.command(14, struct.pack("<IH", BASE, 8))
        crc = [f for f in frames if f[7] == 8][0]
        self.assertEqual(struct.unpack("<I", crc[:4])[0], zlib.crc32(struct.pack("<8H", *range(0x1000, 0x1008))))
        frames = self.command(14)
        self.assertEqual(struct.unpack("<I", [f for f in frames if f[7] == 8][0][:4])[0], self.lib.test_crc())

    def test_read_rejects_bad_ranges_and_lengths(self):
        for address, count in ((BASE - 1, 1), (0xFFFFFFFF, 2), (BASE + SIZE - 1, 2), (BASE, 0), (BASE, 129)):
            self.assertEqual(self.ack(self.command(12, struct.pack("<IH", address, count)))[0], 4)
        self.assertEqual(self.ack(self.command(12, struct.pack("<IH", BASE + SIZE - 1, 1)))[0], 1)

    def test_erase_and_alignment(self):
        for address, count in ((BASE - 4096, 4096), (BASE + 1, 4096), (BASE, 8), (0xFFFFFFFF, 4096)):
            self.assertEqual(self.ack(self.command(13, struct.pack("<IH", address, count)))[0], 4)
        self.assertEqual(self.lib.test_erases(), 0)
        self.assertEqual(self.ack(self.command(13, struct.pack("<IH", BASE, 4096)))[0], 1)
        self.assertEqual(self.lib.test_erases(), 1)
        self.assertEqual(self.ack(self.command(0))[0], 1)
        self.begin()
        self.assertEqual(self.ack(self.command(0))[0], 4)

    def test_write_sequence_duplicates_and_commit(self):
        self.begin()
        self.block()
        self.assertEqual(struct.unpack("<H", self.ack(self.data(0, [0, 1]))[4:6])[0], 2)
        self.assertEqual(struct.unpack("<H", self.ack(self.data(0, [0, 1]))[4:6])[0], 2)
        self.assertEqual(struct.unpack("<H", self.ack(self.block())[4:6])[0], 2)
        for offset, tag in ((4, 21), (2, 22), (0, 21)):
            ack = self.ack(self.data(offset, [2, 3], tag))
            self.assertEqual((ack[0], ack[2]), (3, 11))
        self.assertEqual(self.ack(self.command(3, struct.pack("<I", 0), 21))[0], 4)
        for offset in (2, 4, 6): self.data(offset, [offset, offset + 1])
        crc = zlib.crc32(struct.pack("<8H", *range(8)))
        payload = struct.pack("<I", crc)
        self.assertEqual(self.ack(self.command(3, payload, 21))[0], 1)
        self.assertEqual(self.ack(self.command(3, payload, 21))[0], 1)
        self.assertEqual((self.lib.test_writes(), self.lib.test_write_length()), (1, 8))
        status = self.command(9)[0]
        self.assertEqual(struct.unpack("<I", status[2:6])[0], 8)
        self.assertEqual(self.ack(self.data(0, [0, 1]))[0], 4)

    def test_write_rejects_invalid_configuration_and_overflow(self):
        self.assertEqual(self.ack(self.data(0, [1, 2]))[0], 4)
        self.begin()
        for address, count in ((BASE - 8, 8), (BASE, 0), (BASE + 1, 8), (BASE, 7), (BASE, 136), (0xFFFFFFFF, 8)):
            self.assertEqual(self.ack(self.block(address=address, count=count))[0], 4)
        self.block()
        self.assertEqual(self.ack(self.frame(0x203, struct.pack("<HHHBB", 0, 1, 2, 21, 3)))[2], 12)
        for offset in (0, 2, 4, 6): self.data(offset, [offset, offset + 1])
        self.assertEqual(self.ack(self.data(8, [8, 9]))[2], 12)
        self.assertEqual(self.lib.test_writes(), 0)

    def test_adjacent_blocks_and_last_flash_unit(self):
        self.begin()
        self.write_block([0x1234] * 8, tag=21)
        self.write_block([0xABCD] * 8, tag=22, address=BASE + 8)
        self.write_block([0xFEDC] * 8, tag=23, address=BASE + SIZE - 8)
        self.assertEqual((self.lib.test_writes(), self.lib.test_write_length()), (3, 8))
        frames = self.command(12, struct.pack("<IH", BASE, 16))
        data = [f for f in frames if f[7] == 7]
        words = [word for f in data for word in struct.unpack("<HH", f[2:6])]
        self.assertEqual(words, [0x1234] * 8 + [0xABCD] * 8)

    def test_keep_alive_during_update_and_run_rejection(self):
        self.assertEqual(self.ack(self.command(5))[0], 2)
        self.begin()
        for _ in range(3):
            self.advance(900)
            self.assertEqual(self.ack(self.command(11))[0], 1)
            self.assertEqual(self.lib.test_state(), 2)
        self.assertEqual(self.ack(self.command(5))[0], 4)
        self.assertEqual(self.lib.test_starts(), 0)

    def test_reset_acknowledged_before_reset(self):
        self.assertEqual(self.ack(self.command(6))[0], 1)
        self.assertEqual(self.lib.test_resets(), 0)
        self.lib.test_step()
        self.assertEqual(self.lib.test_resets(), 1)

    def test_crc_failure_then_recovery(self):
        self.begin()
        self.block()
        for offset in (0, 2, 4, 6): self.data(offset, [0, 0])
        for _ in range(3): self.assertEqual(self.ack(self.command(3, struct.pack("<I", 0), 21))[0], 3)
        self.assertEqual(self.ack(self.command(3, struct.pack("<I", 0), 21))[0], 2)
        self.assertEqual(self.lib.test_writes(), 0)
        self.lib.test_step()
        self.assertEqual(self.lib.test_state(), 1)

    def test_finish_commits_actual_flash_crc(self):
        self.begin()
        self.write_block()
        crc = self.lib.test_crc()
        self.assertEqual(self.ack(self.command(4, struct.pack("<I", crc)))[0], 1)
        self.assertEqual(self.lib.test_saved_crc(), crc)
        self.assertEqual(self.lib.test_state(), 4)

    def test_finish_rejects_pending_block_or_bad_crc(self):
        self.begin()
        self.block()
        self.assertEqual(self.ack(self.command(4, struct.pack("<I", 0)))[0], 4)
        self.write_block(tag=22)
        self.assertEqual(self.ack(self.command(4, struct.pack("<I", self.lib.test_crc() ^ 1)))[2], 7)
        self.assertEqual(self.lib.test_metadata_erases(), 0)

    def test_metadata_failures(self):
        for failure in (1, 2):
            self.lib.test_reset(0)
            self.begin()
            self.write_block()
            self.lib.test_metadata_failure(failure)
            self.assertEqual(self.ack(self.command(4, struct.pack("<I", self.lib.test_crc())))[2], 2)
            self.assertEqual(self.lib.test_state(), 5)

    def test_malformed_frames_and_unknown_commands(self):
        for can_id, payload in ((0x201, b"\0" * 7), (0x203, b"\0" * 7), (0x202, b"\0" * 3), (0x201, b"\0" * 9)):
            self.assertEqual(self.frame(can_id, payload), [])
        self.assertEqual(self.ack(self.command(255))[0], 4)
        self.assertEqual(self.lib.test_writes(), 0)

    def test_tx_timeout_prevents_erase(self):
        self.lib.test_tx_busy(1)
        self.assertEqual(self.command(0), [])
        self.assertEqual(self.lib.test_erases(), 0)
        self.lib.test_tx_busy(0)
        system = [f for f in self.command(9) if f[7] == 2][0]
        self.assertEqual(system[5], 13)


@contextmanager
def firmware_library(compiler):
    """Share the real firmware test harness with the Python client tests."""
    os.environ["PATH"] = str(Path(compiler).parent) + os.pathsep + os.environ.get("PATH", "")
    with tempfile.TemporaryDirectory(prefix="bootloader-commands-") as directory, ExitStack() as cleanup:
        build = Path(directory)
        (build / "host_types.h").write_text(HOST_TYPES)
        (build / "device.h").write_text("void SysCtl_resetDevice(void);\n")
        (build / "BCAN.h").write_text(CAN_HEADER)
        (build / "BRD_CAN.h").write_text('#include "BCAN.h"\n#define CAN_INSTANCE_1 0\nextern CanHandle_t BRD_CAN_HANDLE[1];\n')
        (build / "harness.c").write_text(HARNESS)
        includes = [build] + [ROOT / p for p in ("ASW/MAIN", "ASW/BL", "ASW/APP_IF", "BSW/BCRC", "BSW/BFLASH", "BSW/TMR")]
        sources = [ROOT / p for p in ("ASW/APP_IF/APP_IF.c", "ASW/BL/BL_if.c", "ASW/BL/BL_sm.c", "ASW/BL/BL_clstr.c", "BSW/BCRC/BCRC.c")]
        library = build / ("commands.dll" if os.name == "nt" else "commands.so")
        command = [compiler, "-std=c99", "-Wall", "-Wextra", "-Werror", "-Wno-attributes",
                   "-Wno-unknown-pragmas", "-shared", "-fPIC", "-include", str(build / "host_types.h")]
        for include in includes: command.extend(["-I", str(include)])
        subprocess.run(command + list(map(str, sources)) + [str(build / "harness.c"), "-o", str(library)], check=True)
        lib = ctypes.CDLL(str(library))
        if os.name == "nt":
            import _ctypes
            cleanup.callback(_ctypes.FreeLibrary, lib._handle)
        lib.test_frame.argtypes = [ctypes.c_uint32, ctypes.c_char_p, ctypes.c_uint16]
        lib.test_reply.argtypes = [ctypes.c_uint, ctypes.POINTER(ctypes.c_ubyte)]
        lib.test_advance.argtypes = [ctypes.c_uint32]
        for name in ("test_crc", "test_saved_crc", "test_write_length"):
            getattr(lib, name).restype = ctypes.c_uint32
        yield lib


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", default=os.environ.get("CC", "gcc"))
    args = parser.parse_args()
    compiler = shutil.which(args.cc)
    if compiler is None: parser.error("GCC is required; pass --cc with its executable path")
    with firmware_library(compiler) as lib:
        Commands.lib = lib
        result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(Commands))
        if not result.wasSuccessful(): raise SystemExit(1)


if __name__ == "__main__":
    main()
