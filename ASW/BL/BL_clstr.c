/* Application flash cluster buffer. Counts are 16-bit words. */
#include <string.h>
#include "BL_clstr.h"
#include "BL_img.h"
#include "BCRC.h"
#include "BootloaderDefs.h"

#pragma DATA_SECTION(F_blockBuffer, "LNK_blockBufferSection");
#pragma DATA_ALIGN(F_blockBuffer, 8);
static Uint16_t F_blockBuffer[BL_BLOCK_BUFFER_LENGTH];
static Uint32_t F_blockAddress;
static Uint16_t F_clusterStart;
static Uint16_t F_clusterLength;
static Uint16_t F_loaded;
static Uint16_t F_blockLength;

void BL_clstrInit(void)
{
    memset(F_blockBuffer, 0xFFFF, sizeof(F_blockBuffer));
    F_blockAddress = 0uL;
    F_clusterStart = 0u;
    F_clusterLength = 0u;
    F_loaded = 0u;
    F_blockLength = 0u;
}

Uint16_t BL_clstrConfig(ClstrConfig_t *clstr)
{
    Uint16_t start;
    Uint16_t blockLength;
    Uint32_t address;
    if ((clstr == NULL) || !BL_imgRangeValid(clstr->address, clstr->length)) return FAIL;
    start = (Uint16_t)(clstr->address & 7uL);
    if (clstr->length > (BL_BLOCK_BUFFER_LENGTH - start)) return FAIL;
    address = clstr->address & ~7uL;
    blockLength = (Uint16_t)((start + clstr->length + 7u) & ~7u);
    if (!BL_imgRangeValid(address, blockLength)) return FAIL;
    BL_clstrInit();
    F_blockAddress = address;
    F_clusterStart = start;
    F_clusterLength = clstr->length;
    F_blockLength = blockLength;
    clstr->crc = BL_CLSTR_INITIAL_CRC;
    return OK;
}

Uint16_t BL_clstrLoad(ClstrConfig_t *clstr, Uint16_t *dataWords, Uint16_t dataLen)
{
    if ((clstr == NULL) || (dataWords == NULL) || (dataLen == 0u) ||
        (F_clusterLength == 0u) || (clstr->address != F_blockAddress + F_clusterStart) ||
        (clstr->length != F_clusterLength) || (dataLen > F_clusterLength - F_loaded)) return FAIL;
    memcpy(F_blockBuffer + F_clusterStart + F_loaded, dataWords, dataLen * sizeof(*dataWords));
    F_loaded += dataLen;
    return OK;
}

void BL_clstrCrcCompute(ClstrConfig_t *clstr)
{
    if ((clstr != NULL) && (F_clusterLength != 0u) && (F_loaded == F_clusterLength))
        clstr->crc = BCRC_compute(F_blockBuffer + F_clusterStart, F_clusterLength, BL_CLSTR_INITIAL_CRC);
}

RAMFUNC Uint16_t BL_clstrFlash(void)
{
    Uint16_t result;
    if ((F_clusterLength == 0u) || (F_loaded != F_clusterLength)) return FAIL;
    // Program only the touched 8-word units, never the rest of the 128-word buffer.
    result = BFLASH_bufferWrite(F_blockAddress, F_blockBuffer, F_blockLength);
    if (result == OK) F_clusterLength = 0u;
    return result;
}
