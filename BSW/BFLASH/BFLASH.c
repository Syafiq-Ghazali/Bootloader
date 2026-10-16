/*
 * Flash erase and programming API for the bootloader.
 */

#include "BootloaderDefs.h"
#include "BFLASH.h"
#include "device.h"
#include "F021_F28003x_C28x.h"

static Bool_t F_bufferEmptyCheck            (const Uint16_t *buffer, Uint16_t size);
static void   F_flashError                  (Fapi_StatusType status);
static void   F_prefetchSwWorkaroundDisable (Uint32_t ctrlBase);
static void   F_prefetchSwWorkaroundEnable  (Uint32_t ctrlBase);

RAMFUNC static Bool_t F_bufferEmptyCheck
(
    const Uint16_t *buffer,
    Uint16_t size
)
{
    Uint16_t i;

    for (i = 0u; i < size; i++)
    {
        if (buffer[i] != 0xFFFFu)
        {
            return FALSE;
        }
    }

    return TRUE;
}

RAMFUNC static void F_flashError
(
    Fapi_StatusType status
)
{
    (void)status;
    while (1)
    {
    }
}

RAMFUNC static void F_prefetchSwWorkaroundDisable
(
    Uint32_t ctrlBase
)
{
    Flash_disablePrefetch(ctrlBase);
    FLASH_DELAY_CONFIG;
}

RAMFUNC static void F_prefetchSwWorkaroundEnable
(
    Uint32_t ctrlBase
)
{
    Flash_enablePrefetch(ctrlBase);
    FLASH_DELAY_CONFIG;
}

RAMFUNC void BFLASH_initialize
(
    void
)
{
    Fapi_StatusType status;

    Flash_initModule(FLASH0CTRL_BASE, FLASH0ECC_BASE, BFLASH_NUM_WAIT_STATES);

    status = Fapi_initializeAPI(F021_CPU0_BASE_ADDRESS, DEVICE_SYSCLK_FREQ / 1000000u);
    if (status != Fapi_Status_Success)
    {
        F_flashError(status);
    }

    F_prefetchSwWorkaroundDisable(FLASH0CTRL_BASE);

    status = Fapi_setActiveFlashBank(Fapi_FlashBank0);
    if (status != Fapi_Status_Success)
    {
        F_flashError(status);
    }

    F_prefetchSwWorkaroundEnable(FLASH0CTRL_BASE);
}

RAMFUNC static Uint16_t F_sectorErase
(
    BflashSector_t sector
)
{
    Fapi_StatusType retStatus;
    Fapi_FlashStatusType flashStatus;
    Fapi_FlashStatusWordType flashStatusWord;
    Uint32_t sectorStartAddress = BFLASH_sectorAddressGet(sector);

    if (((sectorStartAddress & (BFLASH_SECTOR_SIZE_WORDS - 1uL)) != 0uL) ||
        (sectorStartAddress < BFLASH_START) ||
        (sectorStartAddress > BFLASH_END))
    {
        return FAIL;
    }

    retStatus = Fapi_issueAsyncCommand(Fapi_ClearMore);
    while (Fapi_checkFsmForReady() != Fapi_Status_FsmReady)
    {
    }

    if (retStatus != Fapi_Status_Success)
    {
        return FAIL;
    }

    retStatus = Fapi_issueAsyncCommandWithAddress(Fapi_EraseSector,
                                                  (Uint32_t *)sectorStartAddress);
    while (Fapi_checkFsmForReady() != Fapi_Status_FsmReady)
    {
    }

    if (retStatus != Fapi_Status_Success)
    {
        return FAIL;
    }

    flashStatus = Fapi_getFsmStatus();
    if (flashStatus != 0u)
    {
        return FAIL;
    }

    retStatus = Fapi_doBlankCheck((Uint32_t *)sectorStartAddress,
                                  BFLASH_SECTOR_SIZE_LONG_WORDS,
                                  &flashStatusWord);
    if (retStatus != Fapi_Status_Success)
    {
        return FAIL;
    }

    return OK;
}

RAMFUNC static Uint16_t F_bufferWrite
(
    Uint32_t flashAddr,
    Uint16_t *buffer,
    Uint32_t bufferLen
)
{
    Fapi_StatusType retStatus;
    Fapi_FlashStatusType flashStatus;
    Fapi_FlashStatusWordType flashStatusWord;
    Uint32_t *buffer32 = (Uint32_t *)buffer;
    Uint32_t flashProgramStartAddress;
    Uint32_t i;

    if ((buffer == NULL) || (bufferLen == 0uL) ||
        (flashAddr < BFLASH_START) || (flashAddr > BFLASH_END) ||
        (bufferLen > BFLASH_END - flashAddr + 1uL) ||
        (((Uint32_t)buffer & 0x7uL) != 0uL) ||
        ((flashAddr & 0x7uL) != 0uL) ||
        ((bufferLen & 0x7uL) != 0uL))
    {
        return FAIL;
    }

    for (i = 0u, flashProgramStartAddress = flashAddr;
         flashProgramStartAddress < (flashAddr + bufferLen);
         i += BFLASH_BUFFER_SIZE_WORDS, flashProgramStartAddress += BFLASH_BUFFER_SIZE_WORDS)
    {
        if (F_bufferEmptyCheck(buffer + i, BFLASH_BUFFER_SIZE_WORDS) == FALSE)
        {
            retStatus = Fapi_issueProgrammingCommand((Uint32_t *)flashProgramStartAddress,
                                                     buffer + i,
                                                     BFLASH_BUFFER_SIZE_WORDS,
                                                     0,
                                                     0,
                                                     Fapi_AutoEccGeneration);
            while (Fapi_checkFsmForReady() == Fapi_Status_FsmBusy)
            {
            }

            if (retStatus != Fapi_Status_Success)
            {
                return FAIL;
            }

            flashStatus = Fapi_getFsmStatus();
            if (flashStatus != 0u)
            {
                return FAIL;
            }

            retStatus = Fapi_doVerify((Uint32_t *)flashProgramStartAddress,
                                      4u,
                                      buffer32 + (i / 2u),
                                      &flashStatusWord);
            if (retStatus != Fapi_Status_Success)
            {
                return FAIL;
            }
        }
    }

    return OK;
}

/* ISRs execute from flash, so mask them while the flash FSM is busy. */
RAMFUNC Uint16_t BFLASH_sectorErase(BflashSector_t sector)
{
    Uint16_t interruptState = __disable_interrupts();
    Uint16_t result = F_sectorErase(sector);
    if ((interruptState & 1u) == 0u) __enable_interrupts();
    return result;
}

RAMFUNC Uint16_t BFLASH_bufferWrite(Uint32_t flashAddr, Uint16_t *buffer, Uint32_t bufferLen)
{
    Uint16_t interruptState = __disable_interrupts();
    Uint16_t result = F_bufferWrite(flashAddr, buffer, bufferLen);
    if ((interruptState & 1u) == 0u) __enable_interrupts();
    return result;
}
