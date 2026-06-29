/*
 * Basic CAN driver for the bootloader.
 */

#include "BCAN.h"

static void   F_interruptCallback  (CanHandle_t *handle);
static void   F_canMsgDbInitialize (CanHandle_t *handle);
static Bool_t F_canRxMsgSave       (CanHandle_t *handle, Uint32_t objId, CanMsg_t *canMsg);
static void   F_initCanGpio        (const CanGpioConfig_t *config);

static CanMsg_t F_rxQueueBuffer[CAN_MAX_RX_QUEUE_SIZE];
static volatile Bool_t F_txMsgBusyFlags[CAN_TX_MSG_OBJ_SIZE] = {FALSE};
static volatile Uint16_t F_txCanHwBuf = CAN_TX_MSG_OBJ_SIZE;

void BCAN_init
(
    CanHandle_t *handle
)
{
    if (handle == NULL)
    {
        return;
    }

    F_initCanGpio(handle->txGpioConfig);
    F_initCanGpio(handle->rxGpioConfig);

    CAN_initModule(handle->base);
    F_canMsgDbInitialize(handle);

    CAN_setBitRate(handle->base, DEVICE_SYSCLK_FREQ, handle->bitRate, handle->bitTime);
    CAN_enableInterrupt(handle->base, CAN_INT_IE0 | CAN_INT_ERROR | CAN_INT_STATUS);
    CAN_enableAutoBusOn(handle->base);

    Interrupt_register(handle->interruptNumber, handle->interruptHandler);
    handle->interruptCallback = F_interruptCallback;
    Interrupt_enable(handle->interruptNumber);
    CAN_enableGlobalInterrupt(handle->base, CAN_GLOBAL_INT_CANINT0);

    CAN_startModule(handle->base);
    CanMsgQueue_init(&handle->rxQueue, F_rxQueueBuffer, CAN_MAX_RX_QUEUE_SIZE);
}

static void F_initCanGpio
(
    const CanGpioConfig_t *config
)
{
    if (config == NULL)
    {
        return;
    }

    GPIO_setPinConfig(config->pinConfig);
    GPIO_setPadConfig(config->pinNumber, config->pinType);
    GPIO_setQualificationMode(config->pinNumber, config->qualification);
    GPIO_setDirectionMode(config->pinNumber, config->direction);
}

static void F_canMsgDbInitialize
(
    CanHandle_t *handle
)
{
    Uint32_t objId;

    for (objId = CAN_TX_MSG_OBJ_ID_START; objId <= CAN_TX_MSG_OBJ_ID_END; objId++)
    {
        CAN_setupMessageObject(handle->base,
                               objId,
                               CAN_ID_TX_DUMMY,
                               handle->frameType,
                               CAN_MSG_OBJ_TYPE_TX,
                               CAN_DEFAULT_ID_MASK,
                               CAN_MSG_OBJ_TX_INT_ENABLE,
                               CAN_DEFAULT_LEN);
    }

    for (objId = CAN_RX_MSG_OBJ_ID_START; objId <= CAN_RX_MSG_OBJ_ID_END; objId++)
    {
        CAN_setupMessageObject(handle->base,
                               objId,
                               handle->msgIdTemplate,
                               handle->frameType,
                               CAN_MSG_OBJ_TYPE_RX,
                               handle->mask,
                               (CAN_MSG_OBJ_USE_ID_FILTER | CAN_MSG_OBJ_RX_INT_ENABLE),
                               CAN_DEFAULT_LEN);
    }
}

CanStatus_t BCAN_tx
(
    CanHandle_t *handle,
    CanMsg_t *canMsg
)
{
    Uint16_t objId = CAN_TX_MSG_OBJ_ID_START;

    if ((handle == NULL) || (canMsg == NULL) || (canMsg->bufLen > CAN_DEFAULT_LEN))
    {
        return CAN_STATUS_TX_FAILED;
    }

    if (F_txCanHwBuf == 0u)
    {
        return CAN_STATUS_TX_FULL;
    }

    while ((objId <= CAN_TX_MSG_OBJ_ID_END) && (F_txMsgBusyFlags[objId - 1u] == TRUE))
    {
        objId++;
    }

    if (objId > CAN_TX_MSG_OBJ_ID_END)
    {
        return CAN_STATUS_TX_FULL;
    }

    CAN_setupMessageObject(handle->base,
                           objId,
                           canMsg->id,
                           handle->frameType,
                           CAN_MSG_OBJ_TYPE_TX,
                           CAN_DEFAULT_ID_MASK,
                           CAN_MSG_OBJ_TX_INT_ENABLE,
                           canMsg->bufLen);
    CAN_sendMessage(handle->base, objId, canMsg->bufLen, canMsg->buf);

    F_txMsgBusyFlags[objId - 1u] = TRUE;
    F_txCanHwBuf--;

    return CAN_STATUS_TX_OK;
}

RAMFUNC CanStatus_t BCAN_rx
(
    CanHandle_t *handle,
    CanMsg_t *canMsg
)
{
    CanStatus_t status = CAN_STATUS_RX_EMPTY;

    if ((handle == NULL) || (canMsg == NULL))
    {
        return CAN_STATUS_RX_FAILED;
    }

    Interrupt_disable(handle->interruptNumber);

    if (CanMsgQueue_isEmpty(&handle->rxQueue) == false)
    {
        CanMsgQueue_pop(&handle->rxQueue, canMsg);
        status = CAN_STATUS_RX_OK;
    }

    Interrupt_enable(handle->interruptNumber);

    return status;
}

Bool_t BCAN_txPending
(
    CanHandle_t *handle
)
{
    (void)handle;
    return (F_txCanHwBuf < CAN_TX_MSG_OBJ_SIZE);
}

static Bool_t F_canRxMsgSave
(
    CanHandle_t *handle,
    Uint32_t objId,
    CanMsg_t *canMsg
)
{
    Uint16_t i;
    Uint32_t msgId;
    Uint16_t msgData[CAN_DEFAULT_LEN];
    CAN_MsgFrameType frameType;

    if ((handle == NULL) || (canMsg == NULL))
    {
        return FALSE;
    }

    if (CAN_readMessageWithID(handle->base, objId, &frameType, &msgId, msgData) == false)
    {
        return FALSE;
    }

    canMsg->id = msgId;
    canMsg->bufLen = HWREGH(handle->base + CAN_O_IF2MCTL) & CAN_IF2MCTL_DLC_M;

    for (i = 0u; i < CAN_DEFAULT_LEN; i++)
    {
        canMsg->buf[i] = msgData[i];
    }

    return TRUE;
}

static void F_interruptCallback
(
    CanHandle_t *handle
)
{
    CanMsg_t canMsg = {0};

    if (handle == NULL)
    {
        return;
    }

    handle->interruptCause = CAN_getInterruptCause(handle->base);

    if (handle->interruptCause == CAN_INT_INT0ID_STATUS)
    {
        handle->interruptStatus = CAN_getStatus(handle->base);

        if ((handle->interruptStatus & CAN_STATUS_PERR) == CAN_STATUS_PERR)
        {
            handle->parityErrorCounter++;
        }
        else if ((handle->interruptStatus & CAN_STATUS_BUS_OFF) == CAN_STATUS_BUS_OFF)
        {
            handle->busOffCounter++;
        }
        else if ((handle->interruptStatus & CAN_STATUS_EWARN) == CAN_STATUS_EWARN)
        {
            handle->busErrorWarningCounter++;
        }
        else if ((handle->interruptStatus & CAN_STATUS_EPASS) == CAN_STATUS_EPASS)
        {
            handle->busErrorPassiveCounter++;
        }

        CAN_clearInterruptStatus(handle->base, handle->interruptCause);
    }
    else if ((handle->interruptCause >= CAN_TX_MSG_OBJ_ID_START) &&
             (handle->interruptCause <= CAN_TX_MSG_OBJ_ID_END))
    {
        handle->txMsgCounter++;
        F_txMsgBusyFlags[handle->interruptCause - 1u] = FALSE;
        F_txCanHwBuf++;
        CAN_clearInterruptStatus(handle->base, handle->interruptCause);
    }
    else if ((handle->interruptCause >= CAN_RX_MSG_OBJ_ID_START) &&
             (handle->interruptCause <= CAN_RX_MSG_OBJ_ID_END))
    {
        handle->rxMsgCounter++;

        if (CanMsgQueue_isFull(&handle->rxQueue) == false)
        {
            if (F_canRxMsgSave(handle, handle->interruptCause, &canMsg) == TRUE)
            {
                CanMsgQueue_push(&handle->rxQueue, &canMsg);
            }
        }
        else
        {
            handle->overFlowCounter++;
        }

        CAN_clearInterruptStatus(handle->base, handle->interruptCause);
    }

    CAN_clearGlobalInterruptStatus(handle->base, CAN_GLOBAL_INT_CANINT0);
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP9);
}
