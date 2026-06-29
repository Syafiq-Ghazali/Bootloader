/*
 * Bootloader data cluster handling.
 */

/*=== INCLUDE FILES ==========================================================*/
#include <string.h>

#include "BL_clstr.h"
#include "BCRC.h"
#include "BootloaderDefs.h"

/*=== #DEFINES ===============================================================*/

/*=== TYPE DEFINITIONS =======================================================*/

/*=== ENUMERATIONS ===========================================================*/

/*=== EXTERNALS ==============================================================*/

/*=== PRIVATE FUNCTION PROTOTYPES ============================================*/

/*=== GLOBAL DATA ============================================================*/

/*=== PRIVATE DATA ===========================================================*/

// Block buffer must be 8-word aligned (see linker script)
#pragma DATA_SECTION(F_blockBuffer, "LNK_blockBufferSection");
static Uint16_t F_blockBuffer[BL_BLOCK_BUFFER_LENGTH];
static Uint16_t F_blockBufferIndex = 0;
static Uint32_t F_blockAddress     = 0;

/*
|===============================================================================
|
| Function:         BL_clstrInit
|
| Description:      Initializes the cluster API
|
| Dependencies:
|
| Notes:
|
| Side Effects:
|
| Return Value:
|
|===============================================================================
| Variable     Access  Description  (I=input O=output I/O=in/out)
|-------------------------------------------------------------------------------
|=============================================================================*/

void BL_clstrInit
(
    void
)
{
    // Initialize the block buffer to be all 0xFFFF
    (void)memset((void *)F_blockBuffer, 0xFFFF, BL_BLOCK_BUFFER_LENGTH);
    
    // Initialize block start index and address to 0
    F_blockBufferIndex = 0;
    F_blockAddress = 0;
}

/*
|===============================================================================
|
| Function:         BL_clstrConfig
|
| Description:      Configures the block buffer for a new cluster load sequence
|
| Dependencies:
|
| Notes:
|
| Side Effects:
|
| Return Value:     OK or FAIL
|
|===============================================================================
| Variable   Access  Description  (I=input O=output I/O=in/out)
|-------------------------------------------------------------------------------
| clstr      I       Cluster config struct
|=============================================================================*/

Uint16_t BL_clstrConfig
(
    ClstrConfig_t *clstr
)
{
    Uint16_t ret = FAIL;
    Uint32_t blockAddress = 0;
    Uint16_t blockBufferIndex = 0;

    // Initialize the block buffer index to be the offset between
    // the cluster and block start addresses
    blockBufferIndex = clstr->address & (0x7uL);

    // Block buffer holds 128 words of data
    // Block should start to be written at an 8-word boundary
    blockAddress = clstr->address & (~0x7uL);

    // Check to see if cluster will fit inside the block buffer,
    // considering that F_blockBufferIndex may be non-zero
    // The hex file ensures this is satisfied, but this is done just in case
    if (blockBufferIndex + clstr->length - 1 < BL_BLOCK_BUFFER_LENGTH)
    {
        // Assign the block address and buffer start index
        F_blockAddress = blockAddress;
        F_blockBufferIndex = blockBufferIndex;

        // (Re)initialize the block buffer to be all 0xFFFF
        (void)memset((void *)F_blockBuffer, 0xFFFF, BL_BLOCK_BUFFER_LENGTH);

        ret = OK;
    }
    else
    {
        ret = FAIL;
    }
    
    return ret;
}

/*
|===============================================================================
|
| Function:         BL_clstrLoad
|
| Description:      Loads data from a seperate buffer into the block buffer
|
| Dependencies:
|
| Notes:
|
| Side Effects:
|
| Return Value:     OK or FAIL
|
|===============================================================================
| Variable     Access  Description  (I=input O=output I/O=in/out)
|-------------------------------------------------------------------------------
| clstr        I       Cluster config struct
| dataWords    I       Data buffer, little endian, word-packed (not byte-packed)
| dataLen      I       Number of words in dataWords buffer
|=============================================================================*/

Uint16_t BL_clstrLoad
(
    ClstrConfig_t *clstr,
    Uint16_t *dataWords,
    Uint16_t dataLen
)
{
    Uint16_t ret = FAIL;
    
    const Uint16_t blockBufferIndexAfterLoad = F_blockBufferIndex + dataLen;
    
    // The host should not send data that overflows the block buffer
    const Bool_t blockBufferNotOverflow = (blockBufferIndexAfterLoad - 1) < BL_BLOCK_BUFFER_LENGTH;
    // The host should also not send data that exceeds the configured cluster length
    
    // Make this true for now, as we are sending data messages that are always 8 bytes long
    // Can modify this statement later down the line if algorithm requires this modification
    // Leave this code commented for now, but don't remove it
    // const Uint16_t finalBlockBufferIndex = (clstr->address + clstr->length) - F_blockAddress;
    const Bool_t clusterNotOverflow = TRUE; // (blockBufferIndexAfterLoad - 1) < finalBlockBufferIndex;
    
    if (blockBufferNotOverflow && clusterNotOverflow)
    {
        memcpy(F_blockBuffer + F_blockBufferIndex, dataWords, dataLen);
        F_blockBufferIndex = blockBufferIndexAfterLoad;
        ret = OK;
    }
    else
    {
        ret = FAIL;
    }

    return ret;
}

/*
|===============================================================================
|
| Function:         BL_clstrCrcCompute
|
| Description:      Computes the CRC-32 value of the cluster inside the block buffer
|
| Dependencies:
|
| Notes:
|
| Side Effects:     Crc value inside cluster config struct is updated
|
| Return Value:     
|
|===============================================================================
| Variable     Access  Description  (I=input O=output I/O=in/out)
|-------------------------------------------------------------------------------
| clstr        I/O     Cluster config struct 
|=============================================================================*/

void BL_clstrCrcCompute
(
    ClstrConfig_t *clstr
)
{
    Uint16_t clusterStartIndex = clstr->address - F_blockAddress;
    clstr->crc = BCRC_compute(F_blockBuffer + clusterStartIndex,
                              clstr->length,
                              BL_CLSTR_INITIAL_CRC);
}

/*
|===============================================================================
|
| Function:         BL_clstrFlash
|
| Description:      Flashes the block buffer into flash memory
|
| Dependencies:     BFLASH must be initialized, and app sectors must be erased first
|
| Notes:            RAMFUNC attribute is installed as this function modifies flash 
|                   memory, hence must be exectued from RAM
|
| Side Effects:
|
| Return Value:     OK or FAIL
|
|===============================================================================
| Variable     Access  Description  (I=input O=output I/O=in/out)
|-------------------------------------------------------------------------------
| clstr        I       Cluster config struct
|=============================================================================*/

RAMFUNC Uint16_t BL_clstrFlash
(
    void
)
{
    return BFLASH_bufferWrite(F_blockAddress, F_blockBuffer, BL_BLOCK_BUFFER_LENGTH);
}

/*=== End of File ============================================================*/
