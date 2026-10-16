/*
 * Application image handling.
 */

/*=== INCLUDE FILES ==========================================================*/

#include <string.h>

#include "BL_img.h"
#include "BCRC.h"

/*=== #DEFINES ===============================================================*/

/*=== TYPE DEFINITIONS =======================================================*/

/*=== ENUMERATIONS ===========================================================*/

/*=== EXTERNALS ==============================================================*/

/*=== PRIVATE FUNCTION PROTOTYPES ============================================*/

/*=== GLOBAL DATA ============================================================*/

/*=== PRIVATE DATA ===========================================================*/

#pragma DATA_ALIGN(F_metadataBuffer, 8);
static Uint16_t F_metadataBuffer[16];

Bool_t BL_imgRangeValid(Uint32_t address, Uint32_t wordCount)
{
    // Subtraction avoids overflow from an untrusted address + length.
    return (wordCount != 0uL) &&
           (address >= BL_IMG_APPLICATION_BEGIN_ADDR) && (address <= BFLASH_END) &&
           (wordCount <= (BFLASH_END - address + 1uL));
}

Uint16_t BL_imgRead(Uint32_t address, Uint16_t *data, Uint16_t wordCount)
{
    Uint16_t i;
    const volatile Uint16_t *flash = (const volatile Uint16_t *)address;
    if ((data == NULL) || !BL_imgRangeValid(address, wordCount)) return FAIL;
    for (i = 0u; i < wordCount; i++) data[i] = flash[i];
    return OK;
}

Uint16_t BL_imgRangeCrcCompute(Uint32_t address, Uint32_t wordCount, Uint32_t *crc)
{
    if ((crc == NULL) || !BL_imgRangeValid(address, wordCount)) return FAIL;
    *crc = BCRC_compute((const Uint16_t *)address, wordCount, BL_IMG_INITIAL_CRC);
    return OK;
}

RAMFUNC Uint16_t BL_imgRangeErase(Uint32_t address, Uint32_t wordCount)
{
    Uint32_t offset;
    if (!BL_imgRangeValid(address, wordCount) ||
        ((address % BFLASH_SECTOR_SIZE_WORDS) != 0uL) ||
        ((wordCount % BFLASH_SECTOR_SIZE_WORDS) != 0uL)) return FAIL;
    for (offset = 0uL; offset < wordCount; offset += BFLASH_SECTOR_SIZE_WORDS)
    {
        BflashSector_t sector = (BflashSector_t)((address + offset - BFLASH_START) /
                                                BFLASH_SECTOR_SIZE_WORDS);
        if (BFLASH_sectorErase(sector) != OK) return FAIL;
    }
    return OK;
}

/*
|===============================================================================
|
| Function:         BL_imgVariableMetadataRead
|
| Description:      Reads the variable metadata from flash memory and 
|                   stores it into the given context struct
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
| Variable          Access  Description  (I=input O=output I/O=in/out)
|-------------------------------------------------------------------------------
| variableMetadata  O       Variable metadata struct
|=============================================================================*/

void BL_imgVariableMetadataRead
(
    ImgVariableMetadata_t *variableMetadata
)
{
    // Copy the variable metadata from flash memory into ctx
    void *variableMetadataAddress = (void *)BFLASH_sectorAddressGet(BL_IMG_VARIABLE_METADATA_SECTOR);
    memcpy(variableMetadata, variableMetadataAddress, sizeof(ImgVariableMetadata_t));
}

/*
|===============================================================================
|
| Function:         BL_imgFixedMetadataRead
|
| Description:      Reades the fixed metadata from flash memory and 
|                   stores it into the given context struct
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
| Variable            Access  Description  (I=input O=output I/O=in/out)
|-------------------------------------------------------------------------------
| ImgFixedMetadata_t  O       Fixed metadata struct
|=============================================================================*/

void BL_imgFixedMetadataRead
(
    ImgFixedMetadata_t *fixedMetadata
)
{
    // Copy the fixed metadata from flash memory into ctx
    void *fixedMetadataAddress = (void *)BFLASH_sectorAddressGet(BL_IMG_FIXED_METADATA_SECTOR);
    memcpy(fixedMetadata, fixedMetadataAddress, sizeof(ImgFixedMetadata_t));
}

/*
|===============================================================================
|
| Function:         BL_imgVariableMetadataErase
|
| Description:      Erases the variable metadata sector in flash memory
|
| Dependencies:
|
| Notes:            The RAMFUNC attribute is installed, as this function is used
|                   to modify flash memory.
|
| Side Effects:
|
| Return Value:     OK or FAIL
|
|===============================================================================
| Variable          Access  Description  (I=input O=output I/O=in/out)
|-------------------------------------------------------------------------------
|=============================================================================*/

RAMFUNC Uint16_t BL_imgVariableMetadataErase
(
    void
)
{
    return BFLASH_sectorErase(BL_IMG_VARIABLE_METADATA_SECTOR);
}

/*
|===============================================================================
|
| Function:         BL_imgFixedMetadataErase
|
| Description:      Erases the fixed metadata sector in flash memory
|
| Dependencies:
|
| Notes:            RAMFUNC attribute is installed as this function modifies
|                   flash memory.
|
| Side Effects:
|
| Return Value:     OK or FAIL
|
|===============================================================================
| Variable          Access  Description  (I=input O=output I/O=in/out)
|-------------------------------------------------------------------------------
|=============================================================================*/

RAMFUNC Uint16_t BL_imgFixedMetadataErase
(
    void
)
{
    return BFLASH_sectorErase(BL_IMG_FIXED_METADATA_SECTOR);
}

/*
|===============================================================================
|
| Function:         BL_imgVariableMetadataWrite
|
| Description:      Writes the variable metadata located in the given context
|                   struct into its location in flash memory
|
| Dependencies:
|
| Notes:            RAMFUNC attribute is installed as this function modifies
|                   flash memory 
|
| Side Effects:
|
| Return Value:     OK or FAIL
|
|===============================================================================
| Variable          Access  Description  (I=input O=output I/O=in/out)
|-------------------------------------------------------------------------------
| variableMetadata  O       Variable metadata struct
|=============================================================================*/

RAMFUNC Uint16_t BL_imgVariableMetadataWrite
(
    ImgVariableMetadata_t *variableMetadata
)
{
    // Get sector address
    Uint32_t variableMetadataAddress = BFLASH_sectorAddressGet(BL_IMG_VARIABLE_METADATA_SECTOR);

    // Round up the size to write to the next multiple of 8 (required by BFLASH API)
    Uint32_t bufferLen = ((sizeof(*variableMetadata) / sizeof(Uint16_t) + 7u) / 8u) * 8u;
    if ((variableMetadata == NULL) || (bufferLen > 16u)) return FAIL;
    memset(F_metadataBuffer, 0xFFFF, sizeof(F_metadataBuffer));
    memcpy(F_metadataBuffer, variableMetadata, sizeof(*variableMetadata));

    // Write the buffer to flash memory
    return BFLASH_bufferWrite(variableMetadataAddress, F_metadataBuffer, bufferLen);
}


/*
|===============================================================================
|
| Function:         BL_imgFixedMetadataWrite
|
| Description:      Writes the fixed metadata located in the given context
|                   struct into its location in flash memory
|
| Dependencies:
|
| Notes:            RAMFUNC attribute is installed as this function modifies 
|                   flash memory
|
| Side Effects:
|
| Return Value:     OK or FAIL
|
|===============================================================================
| Variable       Access  Description  (I=input O=output I/O=in/out)
|-------------------------------------------------------------------------------
| fixedMetadata  O       Fixed metadata struct
|=============================================================================*/

RAMFUNC Uint16_t BL_imgFixedMetadataWrite
(
    ImgFixedMetadata_t *fixedMetadata
)
{
    // Get sector address
    Uint32_t fixedMetadataAddress = BFLASH_sectorAddressGet(BL_IMG_FIXED_METADATA_SECTOR);

    // Round up the size to write to the next multiple of 8 (required by BFLASH API)
    Uint32_t bufferLen = ((sizeof(*fixedMetadata) / sizeof(Uint16_t) + 7u) / 8u) * 8u;
    if ((fixedMetadata == NULL) || (bufferLen > 16u)) return FAIL;
    memset(F_metadataBuffer, 0xFFFF, sizeof(F_metadataBuffer));
    memcpy(F_metadataBuffer, fixedMetadata, sizeof(*fixedMetadata));

    // Write the buffer to flash memory
    return BFLASH_bufferWrite(fixedMetadataAddress, F_metadataBuffer, bufferLen);
}

/*
|===============================================================================
|
| Function:         BL_imgAppErase
|
| Description:      Erases all of the flash sectors allocated for the
|                   application firmware
|
| Dependencies:
|
| Notes:            RAMFUNC attribute is installed as this function modifies
|                   flash memory
|
| Side Effects:
|
| Return Value:     OK or FAIL
|
|===============================================================================
| Variable        Access  Description  (I=input O=output I/O=in/out)
|-------------------------------------------------------------------------------
|=============================================================================*/

RAMFUNC Uint16_t BL_imgAppErase
(
    void
)
{
    Uint16_t ret = FAIL;
    
    // Number of sectors in the app (rounded up)
    Uint16_t numAppSectors = (Uint16_t)(BFLASH_NUM_SECTORS - BL_IMG_APPLICATION_START_SECTOR);

    // Stop sector (exclusive)
    BflashSector_t stopSector = (BflashSector_t)(BL_IMG_APPLICATION_START_SECTOR + numAppSectors);
    
    // Erase each sector
    BflashSector_t sector = BL_IMG_APPLICATION_START_SECTOR;
    for (sector = BL_IMG_APPLICATION_START_SECTOR; sector < stopSector; sector++)
    {
        ret = BFLASH_sectorErase(sector);
        if (ret == FAIL)
        {
            break;
        }
    }

    return ret;
}

/*
|===============================================================================
|
| Function:         BL_imgCrcCompute
|
| Description:      Computes CRC-32 value of entire application flash region
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
| Variable         Access  Description  (I=input O=output I/O=in/out)
|-------------------------------------------------------------------------------
|=============================================================================*/

Uint32_t BL_imgCrcCompute
(
    void
)
{
    return BCRC_compute((const Uint16_t *)(BL_IMG_APPLICATION_BEGIN_ADDR),
                        BL_IMG_APPLICATION_REGION_SIZE_WORDS,
                        BL_IMG_INITIAL_CRC);
}

/*
|===============================================================================
|
| Function:         BL_imgAppStart
|
| Description:      Starts the application
|
| Dependencies:
|
| Notes:            This function does not return
|
| Side Effects:
|
| Return Value:     
|
|===============================================================================
| Variable     Access  Description  (I=input O=output I/O=in/out)
|-------------------------------------------------------------------------------
|=============================================================================*/
   
void BL_imgAppStart
(
    void
)
{
    // Reset the stack pointer
    asm("       MOV  SP, #0x400");

    // Jump to the application
    asm("       LB   #" STR_VALUE(BL_IMG_APPLICATION_BEGIN_ADDR) );

    // Function does not return, instead goes to the application
}

/*=== End of File ============================================================*/
