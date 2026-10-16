"""Compile the production CRC/state machine and run host-side regression tests.

Usage: python Bootloader/tests/test_crc.py [--cc path/to/gcc]
Only hardware services are stubbed. Host types explicitly model C28x word widths.
Temporary C files and binaries stay outside the CCS project.
"""

import argparse
from contextlib import ExitStack
import ctypes
import os
from pathlib import Path
import random
import shutil
import struct
import subprocess
import tempfile
import zlib


ROOT = Path(__file__).resolve().parents[1]
HOST_TYPES = """
#include <stdbool.h>
#include <stdint.h>
#define _DATATYPE_H_
#define _BCAN_H_
#define __interrupt
#define DINT ((void)0)
typedef bool Bool_t;
typedef uint16_t Uint16_t;
typedef uint32_t Uint32_t;
typedef float Float32_t;
#define TRUE ((Bool_t)1)
#define FALSE ((Bool_t)0)
"""

HARNESS = r"""
#include <assert.h>
#include <string.h>
#include "BL_ctx.h"
#include "BL_sm.h"
#include "TMR.h"

static const Uint32_t actualCrc = 0x12345678UL;
static ImgVariableMetadata_t stored;
static IfMessage_t incoming;
static IfRequestStatus_t lastStatus;
static IfErrorReason_t lastError;
static Uint32_t reportedCrc;
static Bool_t reportedValid;
static unsigned eraseCalls, writeCalls, flashCalls, startCalls;
static Uint16_t eraseResult = OK, writeResult = OK;
static Uint32_t clusterCrc = 0x87654321UL;
static Bool_t imageErased;

void BL_imgVariableMetadataRead(ImgVariableMetadata_t *m) { *m = stored; }
void BL_imgFixedMetadataRead(ImgFixedMetadata_t *m) { memset(m, 0, sizeof(*m)); }
Uint32_t BL_imgCrcCompute(void) { return imageErased ? 0UL : actualCrc; }
Uint16_t BL_imgAppErase(void) { return OK; }
Bool_t BL_imgRangeValid(Uint32_t a, Uint32_t n)
{ return n && a >= BL_IMG_APPLICATION_BEGIN_ADDR && a <= BFLASH_END && n <= BFLASH_END - a + 1; }
Uint16_t BL_imgRead(Uint32_t a, Uint16_t *d, Uint16_t n)
{ for (Uint16_t i = 0; i < n; ++i) d[i] = (Uint16_t)(a + i); return OK; }
Uint16_t BL_imgRangeErase(Uint32_t a, Uint32_t n)
{ (void)a; (void)n; imageErased = TRUE; return OK; }
Uint16_t BL_imgRangeCrcCompute(Uint32_t a, Uint32_t n, Uint32_t *c)
{ (void)a; (void)n; *c = BL_imgCrcCompute(); return OK; }
Uint16_t BL_imgVariableMetadataErase(void) { ++eraseCalls; return eraseResult; }
Uint16_t BL_imgVariableMetadataWrite(ImgVariableMetadata_t *m)
{
    ++writeCalls;
    if (writeResult == OK) stored = *m;
    return writeResult;
}
void BL_imgAppStart(void) { ++startCalls; }
void BL_clstrInit(void) {}
Uint16_t BL_clstrConfig(ClstrConfig_t *c) { (void)c; return OK; }
void BL_clstrCrcCompute(ClstrConfig_t *c) { c->crc = clusterCrc; }
Uint16_t BL_clstrFlash(void) { ++flashCalls; return OK; }
Uint16_t BL_clstrLoad(ClstrConfig_t *c, Uint16_t *d, Uint16_t n)
{ (void)c; (void)d; (void)n; return OK; }
Uint16_t BL_ifRx(IfMessage_t *m) { *m = incoming; return OK; }
Bool_t BL_ifTxPending(void) { return FALSE; }
Uint16_t BL_ifReplyReqStatusTx(IfRequestStatus_t s, IfMux_t m)
{ (void)m; lastStatus = s; return OK; }
Uint16_t BL_ifReplyVersionTx(Uint16_t b, Uint16_t a, Uint16_t h)
{ (void)b; (void)a; (void)h; return OK; }
Uint16_t BL_ifReplySysStatusTx(Uint32_t crc, Bool_t valid, IfErrorReason_t error)
{ reportedCrc = crc; reportedValid = valid; lastError = error; return OK; }
Uint16_t BL_ifReplyTx(const IfReplyMessage_t *r)
{
    if (r->mux == BL_REPLY_MUX_REQSTATUS) { lastStatus = r->requestStatus; lastError = r->errorReason; }
    if (r->mux == BL_REPLY_MUX_SYSSTATUS)
        return BL_ifReplySysStatusTx(r->imageCrc, r->appValid, r->errorReason);
    return OK;
}
void TMR_createAndStart(TmrHandle_t *t, Uint32_t d) { (void)t; (void)d; }
Bool_t TMR_updateAndCheckExpiry(TmrHandle_t *t) { (void)t; return FALSE; }
void SysCtl_resetDevice(void) {}

static void request(IfMux_t mux)
{
    incoming.type = BL_IF_MESSAGE_REQUEST;
    incoming.request.mux = mux;
    assert(BL_smExe() == OK);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    stored.appCrc = strcmp(argv[1], "startup_mismatch") == 0 ? 0UL : actualCrc;
    stored.appSizeWords = 128;
    assert(BL_smInitialize() == OK);
    assert(BL_smExe() == OK);
    assert(BL_smStateRetreive() == BL_STATE_MENU);
    assert(reportedCrc == actualCrc);
    if (strcmp(argv[1], "startup_mismatch") == 0)
    {
        assert(!reportedValid && lastError == BL_ERROR_APP_CRC);
        return 0;
    }
    assert(reportedValid && lastError == BL_ERROR_NONE);
    if (strcmp(argv[1], "startup_valid") == 0) return 0;

    if (strcmp(argv[1], "erase_invalidates") == 0)
    {
        request(BL_REQUEST_MUX_APP_ERASE);
        request(BL_REQUEST_MUX_APP_RUN);
        assert(startCalls == 0 && BL_smStateRetreive() == BL_STATE_MENU);
        assert(lastStatus == BL_REQUEST_STATUS_FAILED);
        return 0;
    }

    incoming.request.appVersion = 7;
    incoming.request.appSize = 128;
    request(BL_REQUEST_MUX_UPDATE_INITIALIZE);
    assert(BL_smStateRetreive() == BL_STATE_UPDATE);

    if (strcmp(argv[1], "cluster_retry_reset") == 0)
    {
        for (unsigned cluster = 0; cluster < 2; ++cluster)
        {
            incoming.request.clusterAddress = BL_IMG_APPLICATION_BEGIN_ADDR + cluster * 8;
            incoming.request.clusterLength = 8;
            incoming.request.tag = (Uint16_t)(cluster + 1);
            request(BL_REQUEST_MUX_LOAD_CONFIGURE);
            incoming.type = BL_IF_MESSAGE_DATA;
            incoming.lenBytes = 8;
            assert(BL_smExe() == OK);
            assert(BL_smExe() == OK);
            incoming.request.clusterCrc = 0;
            for (unsigned i = 0; i < BL_CLUSTER_CRC_MAX_RETRIES; ++i)
            {
                request(BL_REQUEST_MUX_BUFFER_FLASH);
                assert(lastStatus == BL_REQUEST_STATUS_RETRY);
                assert(flashCalls == cluster);
            }
            incoming.request.clusterCrc = clusterCrc;
            request(BL_REQUEST_MUX_BUFFER_FLASH);
            assert(lastStatus == BL_REQUEST_STATUS_COMPLETE);
            assert(flashCalls == cluster + 1);
        }
        return 0;
    }
    if (strcmp(argv[1], "cluster_retry_exhausted") == 0)
    {
        incoming.request.clusterAddress = BL_IMG_APPLICATION_BEGIN_ADDR;
        incoming.request.clusterLength = 8;
        request(BL_REQUEST_MUX_LOAD_CONFIGURE);
        incoming.type = BL_IF_MESSAGE_DATA;
        incoming.lenBytes = 8;
        assert(BL_smExe() == OK);
        assert(BL_smExe() == OK);
        incoming.request.clusterCrc = 0;
        for (unsigned i = 0; i <= BL_CLUSTER_CRC_MAX_RETRIES; ++i)
            request(BL_REQUEST_MUX_BUFFER_FLASH);
        assert(lastStatus == BL_REQUEST_STATUS_FAILED && flashCalls == 0);
        assert(BL_smStateRetreive() == BL_STATE_ERROR);
        assert(BL_smExe() == OK);
        assert(lastError == BL_ERROR_CLUSTER_CRC);
        return 0;
    }

    incoming.request.appCrc = actualCrc;
    if (strcmp(argv[1], "finish_mismatch") == 0) incoming.request.appCrc ^= 1UL;
    if (strcmp(argv[1], "metadata_erase_failure") == 0) eraseResult = FAIL;
    if (strcmp(argv[1], "metadata_write_failure") == 0) writeResult = FAIL;
    request(BL_REQUEST_MUX_UPDATE_FINISH);

    if (strcmp(argv[1], "finish_valid") == 0)
    {
        assert(lastStatus == BL_REQUEST_STATUS_COMPLETE);
        assert(BL_smStateRetreive() == BL_STATE_RESET);
        assert(eraseCalls == 1 && writeCalls == 1);
        assert(stored.appCrc == actualCrc && stored.appVersion == 7);
    }
    else
    {
        assert(lastStatus == BL_REQUEST_STATUS_FAILED);
        assert(BL_smStateRetreive() == BL_STATE_ERROR);
        assert(stored.appVersion != 7);
        if (strcmp(argv[1], "finish_mismatch") == 0)
            assert(eraseCalls == 0 && writeCalls == 0);
        else if (eraseResult == FAIL)
            assert(eraseCalls == 1 && writeCalls == 0);
        else
            assert(eraseCalls == 1 && writeCalls == 1);
        assert(BL_smExe() == OK);
        assert(lastError == (strcmp(argv[1], "finish_mismatch") == 0
                            ? BL_ERROR_APP_CRC : BL_ERROR_FLASH));
        assert(reportedCrc == actualCrc);
    }
    return 0;
}
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", default=os.environ.get("CC", "gcc"))
    args = parser.parse_args()
    compiler = shutil.which(args.cc)
    if compiler is None:
        parser.error("GCC is required; pass --cc with its executable path")
    # MinGW compiler subprocesses also need the toolchain DLLs on PATH.
    os.environ["PATH"] = str(Path(compiler).parent) + os.pathsep + os.environ.get("PATH", "")

    with tempfile.TemporaryDirectory(prefix="bootloader-crc-") as temporary, ExitStack() as cleanup:
        build = Path(temporary)
        (build / "host_types.h").write_text(HOST_TYPES)
        (build / "device.h").write_text("void SysCtl_resetDevice(void);\n")
        (build / "harness.c").write_text(HARNESS)
        includes = [build, ROOT / "ASW/MAIN", ROOT / "ASW/BL",
                    ROOT / "ASW/APP_IF",
                    ROOT / "BSW/BCRC", ROOT / "BSW/BFLASH", ROOT / "BSW/TMR",
                    ROOT / "BSW/BCAN"]
        command = [compiler, "-std=c99", "-Wall", "-Wextra", "-Werror",
                   "-Wno-unknown-pragmas", "-include", str(build / "host_types.h")]
        for directory in includes:
            command.extend(["-I", str(directory)])
        library = build / ("crc.dll" if os.name == "nt" else "crc.so")
        subprocess.run(command + ["-shared", "-fPIC", str(ROOT / "BSW/BCRC/BCRC.c"),
                                  "-o", str(library)], check=True)
        crc = ctypes.CDLL(str(library))
        if os.name == "nt":
            import _ctypes
            cleanup.callback(_ctypes.FreeLibrary, crc._handle)
        u16, u32 = ctypes.c_uint16, ctypes.c_uint32
        crc.BCRC_compute.argtypes = [ctypes.POINTER(u16), u32, u32]
        crc.BCRC_wordAppend.argtypes = [u16, u32]
        crc.BCRC_ffffAdd.argtypes = [u32, u32]
        for name in ("BCRC_compute", "BCRC_wordAppend", "BCRC_ffffAdd"):
            getattr(crc, name).restype = u32

        def compute(words, seed=0):
            return crc.BCRC_compute((u16 * len(words))(*words), len(words), seed)

        # A known vector catches byte-order, polynomial, and XOR mistakes.
        assert compute([0x3231, 0x3433, 0x3635, 0x3837]) == 0x9AE0DAAF
        rng = random.Random(42)
        for count in (0, 1, 2, 7, 128, 1024):
            words = [rng.randrange(65536) for _ in range(count)]
            packed = struct.pack("<" + "H" * count, *words)
            for seed in (0, 0x12345678, 0xFFFFFFFF):
                expected = zlib.crc32(packed, seed)
                assert compute(words, seed) == expected
                accumulated = seed
                for word in words:
                    accumulated = crc.BCRC_wordAppend(word, accumulated)
                assert accumulated == expected
                split = count // 2
                assert compute(words[split:], compute(words[:split], seed)) == expected
                assert crc.BCRC_compute(None, 0, seed) == seed
                assert crc.BCRC_ffffAdd(count, seed) == zlib.crc32(b"\xff\xff" * count, seed)

        # Include the full application range and erased gaps between segments.
        prefix, suffix, gap = [0x1234, 0xABCD], [0, 0xFEDC], 131
        combined = crc.BCRC_ffffAdd(gap, compute(prefix))
        assert compute(suffix, combined) == compute(prefix + [0xFFFF] * gap + suffix)
        region_words = 0xAFFFF + 1 - 0x85000
        assert crc.BCRC_ffffAdd(region_words, 0) == zlib.crc32(b"\xff\xff" * region_words)
        print("CRC vectors, streaming, seeds, word order, and erased gaps: PASS")

        executable = build / ("state_tests.exe" if os.name == "nt" else "state_tests")
        subprocess.run(command + [str(ROOT / "ASW/BL/BL_sm.c"),
                                  str(ROOT / "ASW/APP_IF/APP_IF.c"),
                                  str(build / "harness.c"), "-o", str(executable)], check=True)
        for scenario in ("startup_valid", "startup_mismatch", "finish_valid",
                         "finish_mismatch", "metadata_erase_failure", "metadata_write_failure",
                         "cluster_retry_reset", "cluster_retry_exhausted", "erase_invalidates"):
            subprocess.run([str(executable), scenario], check=True)
            print(f"{scenario}: PASS")

if __name__ == "__main__":
    main()
