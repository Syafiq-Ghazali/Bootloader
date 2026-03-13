/* Include Files */

#include "BFLASH.h"
#include "F021_F28003x_C28x.h"

/* Function */

__attribute__((ramfunc)) void BFLASH_init(void)
{
    Fapi_StatusType status;

    // Initialize the flash peripheral module
    Flash_initModule(FLASH0CTRL_BASE, FLASH0ECC_BASE, BFLASH_WAITSTATES);
    
    // Initialize the Flash API
    // Need to do this every time system frequency or the RWAIT in the FRDCNTL register is changed.
    // RWAIT is the number of wait states added to a flash write/fetch access.
    status = Fapi_initializeAPI(F021_CPU0_BASE_ADDRESS, DEVICE_SYSCLK_FREQ/1000000u);
    if (status != Fapi_Status_Success)
    {
        while (1); 
    }

    // Disable flash prefetch
    Flash_disablePrefetch(FLASH0CTRL_BASE);

    // Force a pipeline flush to ensure that the write to the last register
    // configured occurs before returning
    FLASH_DELAY_CONFIG;

    // Intialize the flash banks and FMC for erase and program operations.
    // Fapi_setActiveFlashBank() sets the flash banks and FSM for
    // further flash operations to be performed on the banks.
    // According to flash API docs, function only needs to be
    // called once, and it can be called with Fapi_FlashBank0.
    // After calling it once, **there is no need to call it 
    // again, even if you want to switch banks**.
    status = Fapi_setActiveFlashBank(Fapi_FlashBank0);
    if (status != Fapi_Status_Success)
    {
        while (1); 
    }

    // Enable flash prefetch
    Flash_enablePrefetch(FLASH0CTRL_BASE);

    // Force a pipeline flush to ensure that the write to the last register
    // configured occurs before returning.
    FLASH_DELAY_CONFIG;
}

void BFLASH_clear(void)
{
    
}

void BFLASH_write(void)
{

}
