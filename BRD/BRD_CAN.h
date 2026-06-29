/*
 * Bootloader board CAN configuration.
 */

#ifndef _BRD_CAN_H_
#define _BRD_CAN_H_

#include "BCAN.h"

#define BRD_CAN0_BASE             (CANA_BASE)
#define BRD_CAN_BIT_RATE          ((Uint32_t)500000u)
#define BRD_CAN_BIT_TIME          (8u)

#define BRD_CAN_TX_GPIO           (4u)
#define BRD_CAN_TX_PINCFG         ((Uint32_t)GPIO_4_CANA_TX)
#define BRD_CAN_RX_GPIO           (5u)
#define BRD_CAN_RX_PINCFG         ((Uint32_t)GPIO_5_CANA_RX)

#define BRD_CAN_MSG_ID_TEMPLATE   (0x200uL)
#define BRD_CAN_MASK              (0x7FCuL)

typedef enum CanInstance_t
{
    CAN_INSTANCE_1,
    CAN_INSTANCE_COUNT,
} CanInstance_t;

extern CanHandle_t BRD_CAN_HANDLE[CAN_INSTANCE_COUNT];

#endif
