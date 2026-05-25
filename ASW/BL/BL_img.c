/*
|===============================================================================
|
| File:         BL_img.c
|
| Project:      DAANAA C2000 BOOTLOADER
|
| Processor:    TI TMS320F28003x
| Compiler:     TI C2000 compiler 22.6.0
|
| Component:    Bootloader Project Component
|
| Description:  Application image handling functions
|
| Copyright:    Copyright (C) 2025 Daanaa Resolution Inc.
|
|               All Rights Reserved. Reproduction or disclosure of this file 
|               or its Contents without the prior written consent of Daanaa 
|               Resolution Inc is prohibited.
|===============================================================================
| Version   Date        Author  Description
|-------------------------------------------------------------------------------
|  1.00   DD-MMM-2025   AP      Initial Release.
|=============================================================================*/

/*=== INCLUDE FILES ==========================================================*/

#include <string.h>

#include "BL_img.h"
#include "CRC.h"

/*=== #DEFINES ===============================================================*/

/*=== TYPE DEFINITIONS =======================================================*/

/*=== ENUMERATIONS ===========================================================*/

/*=== EXTERNALS ==============================================================*/

/*=== PRIVATE FUNCTION PROTOTYPES ============================================*/

/*=== GLOBAL DATA ============================================================*/

/*=== PRIVATE DATA ===========================================================*/

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
    Uint32_t bufferLen = ((sizeof(ImgVariableMetadata_t) + 7) / 8) * 8;
    Uint16_t *buffer = (Uint16_t *)(variableMetadata);
    
    // Note: Up to 7 words of additional memory will be written to flash. This is
    // fine, as this sector is completely dedicated to the variable metadata

    // Write the buffer to flash memory
    return BFLASH_bufferWrite(variableMetadataAddress, buffer, bufferLen);
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
    Uint32_t bufferLen = ((sizeof(ImgFixedMetadata_t) + 7) / 8) * 8;
    Uint16_t *buffer = (Uint16_t *)(fixedMetadata);
 
    // Note: Up to 7 words of additional memory will be written to flash. This is
    // fine, as this sector is completely dedicated to the fixed metadata

    // Write the buffer to flash memory
    return BFLASH_bufferWrite(fixedMetadataAddress, buffer, bufferLen);
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
    return CRC_compute((Uint16_t *)(BL_IMG_APPLICATION_BEGIN_ADDR), 
                       BL_IMG_APPLICATION_REGION_SIZE_WORDS, BL_IMG_INITIAL_CRC);
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
