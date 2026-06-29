/*
 * Bootloader board CAN configuration.
 */

#include "BRD_CAN.h"

static __interrupt void F_canInterruptHandler(void);

static const CanGpioConfig_t F_canTxGpioConfig =
{
    .pinNumber = BRD_CAN_TX_GPIO,
    .pinConfig = BRD_CAN_TX_PINCFG,
    .qualification = GPIO_QUAL_SYNC,
    .pinType = GPIO_PIN_TYPE_STD,
    .direction = GPIO_DIR_MODE_OUT,
};

static const CanGpioConfig_t F_canRxGpioConfig =
{
    .pinNumber = BRD_CAN_RX_GPIO,
    .pinConfig = BRD_CAN_RX_PINCFG,
    .qualification = GPIO_QUAL_SYNC,
    .pinType = GPIO_PIN_TYPE_STD,
    .direction = GPIO_DIR_MODE_IN,
};

CanHandle_t BRD_CAN_HANDLE[CAN_INSTANCE_COUNT] =
{
    [CAN_INSTANCE_1] =
    {
        .base = BRD_CAN0_BASE,
        .txGpioConfig = &F_canTxGpioConfig,
        .rxGpioConfig = &F_canRxGpioConfig,
        .bitRate = BRD_CAN_BIT_RATE,
        .bitTime = BRD_CAN_BIT_TIME,
        .frameType = CAN_MSG_FRAME_STD,
        .msgIdTemplate = BRD_CAN_MSG_ID_TEMPLATE,
        .mask = BRD_CAN_MASK,
        .interruptNumber = INT_CANA0,
        .interruptHandler = F_canInterruptHandler,
    },
};

static __interrupt void F_canInterruptHandler
(
    void
)
{
    CanHandle_t *handle = &BRD_CAN_HANDLE[CAN_INSTANCE_1];

    if (handle->interruptCallback != NULL)
    {
        handle->interruptCallback(handle);
    }
}
