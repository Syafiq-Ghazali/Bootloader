/*
|===============================================================================
|
| File:         BCAN.c
|
| Project:      Agnostic 
|
| Processor:    TI TMS320F28003x
|
| Compiler:     TI C2000 compiler 22.6.0
|
| Component:    BSW Source File
|
| Description:  
|
| Copyright:    Copyright (C) 2025 Daanaa Resolution Inc.
|
|               All Rights Reserved. Reproduction or disclosure of this file 
|               or its Contents without the prior written consent of Daanaa 
|               Resolution Inc is prohibited.
|===============================================================================
| Version   Date          Author  Description
|-------------------------------------------------------------------------------
| 1.00      DD-MMM-2025   AP      Initial Release.
|=============================================================================*/

/*=== INCLUDE FILES ==========================================================*/

#include "BCAN.h"

/*=== #DEFINES ===============================================================*/

#define DEVICE_SYSCLK_FREQ (((uint32_t)((20000000U * 48) / (2 * 4 * 1))))

/*=== TYPE DEFINITIONS =======================================================*/

/*=== ENUMERATIONS ===========================================================*/

/*=== EXTERNALS ==============================================================*/

/*=== PRIVATE FUNCTION PROTOTYPES ============================================*/

static void     F_interruptCallback     (CanHandle_t *handle);
static void     F_canMsgDbInitialize    (CanHandle_t *handle);
static bool     F_canRxMsgSave          (CanHandle_t *handle, uint32_t id,
                                         CanMsg_t *canMsg);
static void     F_initCanGPIO           (void);

/*=== GLOBAL DATA ============================================================*/

/*=== PRIVATE DATA ===========================================================*/

static           CanMsg_t    F_rxQueueBuffer[CAN_MAX_RX_QUEUE_SIZE];
volatile static  bool      F_txMsgBusyFlags[CAN_TX_MSG_OBJ_SIZE] = {FALSE};
volatile static  uint16_t    F_txCanHwBuf = CAN_TX_MSG_OBJ_SIZE;

/*
|===============================================================================
|
| Function:         BCAN_init
|
| Description:      Initialize an CAN instance
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
| handle       I/O     CAN instance handle
|=============================================================================*/

void BCAN_init 
(
    CanHandle_t *handle
)
{
    /* Initialize CAN GPIO pins */
    F_initCanGPIO();
    
    /* Initialize the CAN controller */
    CAN_initModule(handle->base);

    /* Sets up the CAN msg Database */
    F_canMsgDbInitialize(handle);  

    CAN_setBitRate(handle->base, DEVICE_SYSCLK_FREQ, 
                   handle->bitRate, handle->bitTime);

    CAN_enableInterrupt(handle->base, CAN_INT_IE0 | 
                        CAN_INT_ERROR | CAN_INT_STATUS);

    /* Enable auto bus on */
    CAN_enableAutoBusOn(handle->base);

    Interrupt_register(handle->interruptNumber, handle->interruptHandler);
    handle->interruptCallback = F_interruptCallback;
    Interrupt_enable(handle->interruptNumber);
    CAN_enableGlobalInterrupt(handle->base, CAN_GLOBAL_INT_CANINT0);

    CAN_startModule(handle->base);
    CanMsgQueue_init(&handle->rxQueue, F_rxQueueBuffer, CAN_MAX_RX_QUEUE_SIZE);
}

static void F_initCanGPIO(void)
{
    GPIO_setPinConfig(GPIO_4_CANA_TX);
    GPIO_setPadConfig(4u, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(4u, GPIO_QUAL_SYNC);
    GPIO_setDirectionMode(4u, GPIO_DIR_MODE_OUT);

    GPIO_setPinConfig(GPIO_5_CANA_RX);
    GPIO_setPadConfig(5u, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(5u, GPIO_QUAL_SYNC);
    GPIO_setDirectionMode(5u, GPIO_DIR_MODE_OUT);

}

/*
|===============================================================================
|
| Function:         F_canMsgDbInitialize
|
| Description:      Initialize how the messages are organized 
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
| handle       I/O     CAN instance handle
|=============================================================================*/

static void F_canMsgDbInitialize
(
    CanHandle_t *handle
)
{
    /* Organized the Tx messages from 1-16 and Rx from 17 to 32 */
    uint32_t i;
    for (i = CAN_TX_MSG_OBJ_ID_START; i <= CAN_TX_MSG_OBJ_ID_END; i++)
    {
        CAN_setupMessageObject(handle->base, i, CAN_ID_TX_DUMMY, handle->frameType,
                            CAN_MSG_OBJ_TYPE_TX, CAN_DEFAULT_ID_MASK, CAN_MSG_OBJ_TX_INT_ENABLE,
                            CAN_DEFAULT_LEN);
    }

    for (i = CAN_RX_MSG_OBJ_ID_START; i <= CAN_RX_MSG_OBJ_ID_END; i++)
    { 

        /*
         * To set up filtering the handle parameters msgIDTemplate and mask 
         * are used in conjuction where if msgIdTemplate and mask was
         * msgIdTemplate: 11'b0_1001_1001 and mask: 11'b1_1111_0001
         * it would only accept ID's containing 11'b0_1001_xxx1
         */
        CAN_setupMessageObject(handle->base, i, handle->msgIdTemplate, handle->frameType,
                            CAN_MSG_OBJ_TYPE_RX, handle->mask,
                            (CAN_MSG_OBJ_USE_ID_FILTER | CAN_MSG_OBJ_NO_FLAGS |CAN_MSG_OBJ_RX_INT_ENABLE),
                            CAN_DEFAULT_LEN);
    }
}

/*
|===============================================================================
|
| Function:         BCAN_tx
|
| Description:      Send a single Can message
|
| Dependencies:
|
| Notes:           
|
| Side Effects:
|
| Return Value: CAN_STATUS_TX_OK if a message is able to be sent
|               CAN_STATUS_TX_FULL if all available mailboxes are full
|               CAN_STATUS_TX_FAILED if a message is unable to be sent 
|
|===============================================================================
| Variable     Access  Description  (I=input O=output I/O=in/out)
|-------------------------------------------------------------------------------
| handle       I/O     CAN instance handle
| canMsg       I/O     CAN instance CanMsg
|=============================================================================*/

CanStatus_t BCAN_tx
(
    CanHandle_t *handle,
    CanMsg_t *canMsg
)
{
    uint16_t i = CAN_TX_MSG_OBJ_ID_START;

    if (canMsg == NULL)
    {
        return CAN_STATUS_TX_FAILED;
    }

    if  (F_txCanHwBuf == 0u )
    {
        return CAN_STATUS_TX_FULL;
    }

    /* Find a available mailbox */ 
    while ((i <= CAN_TX_MSG_OBJ_ID_END) && ((F_txMsgBusyFlags[i - 1u])))
    {
        i++;
    }

    /* Sets up a Can Message to transmit */
    CAN_setupMessageObject(handle->base, i, canMsg->id, handle->frameType, CAN_MSG_OBJ_TYPE_TX,
                            CAN_DEFAULT_ID_MASK, CAN_MSG_OBJ_TX_INT_ENABLE, canMsg->bufLen);
    CAN_sendMessage(handle->base, i, canMsg->bufLen, canMsg->buf);
    F_txMsgBusyFlags[i - 1u] = TRUE;
    F_txCanHwBuf--;
    return CAN_STATUS_TX_OK;
}

/*
|===============================================================================
|
| Function:     BCAN_rx
|
| Description:  Pollng functon to check if a message is available 
|
| Dependencies: 
|
| Notes:        
|
| Side Effects:
|
| Return Value: CAN_STATUS_RX_OK if a message is located
|               CAN_STATUS_RX_EMPTY if there is no available message 
|
|===============================================================================
| Variable     Access  Description  (I=input O=output I/O=in/out)
|-------------------------------------------------------------------------------
| handle       I/O     CAN instance handle
| canMsg       I/O     CAN instance CanMsg
|===============================================================================*/

__attribute__((ramfunc)) CanStatus_t BCAN_rx
(
    CanHandle_t *handle,
    CanMsg_t *canMsg
)
{
    if (CanMsgQueue_isEmpty(&handle->rxQueue) == FALSE && canMsg)
    {
        CanMsgQueue_pop(&handle->rxQueue, canMsg);
        return CAN_STATUS_RX_OK;
    }
    else
    {
        return CAN_STATUS_RX_EMPTY;
    }
}

/*
|===============================================================================
|
| Function:         F_canRxMsgSave
|
| Description:      Saves a message to the CAN hande struct
|
| Dependencies:
|
| Notes:           
|
| Side Effects:
|
| Return Value: TRUE if a message is located
|               FALSE if there is not able to read a message
|===============================================================================
| Variable     Access  Description  (I=input O=output I/O=in/out)
|-------------------------------------------------------------------------------
| handle       I/O     CAN instance handle
| canMsg       I       CAN instance CanMsg
|=============================================================================*/

static bool F_canRxMsgSave
(
    CanHandle_t *handle,
    uint32_t id,
    CanMsg_t *canMsg
)
{   
    uint16_t i;
    uint32_t msgID; 
    uint16_t msgData[CAN_DEFAULT_LEN];
    CAN_MsgFrameType frameType;

    /* Saves the proper CAN message parameters back to the can message struct */
    if (CAN_readMessageWithID(handle->base, id, &frameType, &msgID, msgData))
    {
        canMsg->bufLen = HWREGH(handle->base + CAN_O_IF2MCTL) & CAN_IF2MCTL_DLC_M;
        canMsg->id = msgID;
        for ( i = 0u; i < 8u; i++ )
        {
            canMsg->buf[i] = msgData[i];
        }
        return TRUE;
    }
    return FALSE;
}

/*
|===============================================================================
|
| Function:        F_interruptCallback
|
| Description:     Call back function when a interrupt triggers
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
| handle       I/O     CAN instance handle
|=============================================================================*/

static void F_interruptCallback
(
    CanHandle_t *handle
)
{
    handle->interruptCause = CAN_getInterruptCause(handle->base);
    CanMsg_t canMsgInst;

    /* 
     * Check if the interrupt was due to a status change
     * increments the status flag counter for each case
     */
    if (handle->interruptCause == CAN_INT_INT0ID_STATUS)
    {
        /* Test the bit for a status change */
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
        else if ((handle->interruptStatus & CAN_STATUS_EPASS) == CAN_STATUS_EWARN)
        {
            handle->busErrorPassiveCounter++;
        }
        CAN_clearInterruptStatus(handle->base, handle->interruptCause);
    }

    /* 
     * Interrupt caused by a Tx message between 1-16
     * Start Callback Function when a message is transmitted
     */
    else if ((handle->interruptCause >= CAN_TX_MSG_OBJ_ID_START) && 
             (handle->interruptCause <= CAN_TX_MSG_OBJ_ID_END))
    {      
        handle->txMsgCounter++;
        F_txMsgBusyFlags[handle->interruptCause - 1u] = FALSE;
        F_txCanHwBuf++;
        CAN_clearInterruptStatus(handle->base, handle->interruptCause);
    }
    /* 
     * Interrupt cause by a Rx message between 17-32
     * Save the message in the handler when a message is received
     * Start Callback Function for when a message is received
     */ 
    else if ((handle->interruptCause >= CAN_RX_MSG_OBJ_ID_START) && 
             (handle->interruptCause <= CAN_RX_MSG_OBJ_ID_END))
    {
        handle->rxMsgCounter++;
        if ( CanMsgQueue_isFull(&handle->rxQueue) == FALSE )
        {
            if (F_canRxMsgSave(handle, handle->interruptCause, &canMsgInst))
            {
                CanMsgQueue_push(&handle->rxQueue, &canMsgInst);
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

/*=== End of File ============================================================*/
