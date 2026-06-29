/*
 * Bootloader board configuration.
 */

#include "BRD.h"
#include "BRD_CAN.h"

void BRD_init
(
    void
)
{
    BCAN_init(&BRD_CAN_HANDLE[CAN_INSTANCE_1]);
}
