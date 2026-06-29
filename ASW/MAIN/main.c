/*
 * Bootloader entry point.
 */

#include "BootloaderDefs.h"
#include "driverlib.h"
#include "device.h"
#include "BRD.h"
#include "BFLASH.h"
#include "TMR.h"
#include "BL_if.h"
#include "BL_clstr.h"
#include "BL_sm.h"

void main
(
    void
)
{
    DISABLE_GLOBAL_INTERRUPT();

    Device_init();
    Device_initGPIO();
    Interrupt_initModule();
    Interrupt_initVectorTable();

    BRD_init();
    BFLASH_initialize();
    TMR_initialize();

    BL_ifInit();
    BL_clstrInit();
    BL_smInitialize();

    ENABLE_GLOBAL_INTERRUPT();
    ENABLE_GLOBAL_REALTIME_INTERRUPT_DBGM();

    BL_smStart();

    while (1)
    {
    }
}
