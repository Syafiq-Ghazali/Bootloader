/*
|===============================================================================
|
| File:         BL_if.c
|
| Project:      DAANAA C2000 BOOTLOADER
|
| Processor:    TI TMS320F28003x
| Compiler:     TI C2000 compiler 22.6.0
|
| Component:    Bootloader Project Component
|
| Description:  Bootloader communication interface API
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
#include "BL_if.h"

/*=== #DEFINES ===============================================================*/

/*=== TYPE DEFINITIONS =======================================================*/

/*=== ENUMERATIONS ===========================================================*/

/*=== EXTERNALS ==============================================================*/

/*=== PRIVATE FUNCTION PROTOTYPES ============================================*/

static Uint16_t F_canEncode(can_t *frame, const IfMessage_t *msg);
static Uint16_t F_canDecode(IfMessage_t *msg, const can_t *frame);

/*=== GLOBAL DATA ============================================================*/

/*=== PRIVATE DATA ===========================================================*/

static IfMessage_t F_ifRxQueueBuffer[BL_IF_RX_QUEUE_LENGTH];
static F_ifQueue_t F_ifRxQueue = {0};

/*
|===============================================================================
|
| Function:         F_canEncode
|
| Description:      Encodes a reply message into a CAN frame
|
| Dependencies:
|
| Notes:            Primarily used to encode reply messages but other
|                   messages can also be encoded (useful for testing)
|
| Side Effects:
|
| Return Value:     OK OR FAIL
|
|===============================================================================
| Variable     Access  Description  (I=input O=output I/O=in/out)
|-------------------------------------------------------------------------------
| frame        O       CAN frame destination
| msg          I       BL_REPLY message source
|=============================================================================*/

static Uint16_t F_canEncode
(
    can_t *frame,
    const IfMessage_t *msg
)
{
    Uint16_t ret = FAIL;

    if ((msg == NULL) || (frame == NULL)) 
    {
        ret = FAIL;
        return ret;
    }
    
    // Encode the CAN payload based on the message type (based on CAN ID)
    switch (msg->type)
    {
        case BL_IF_MESSAGE_REPLY:
        {
            // Message is muxed, so insert payload signals based on the mux
            switch (msg->reply.mux)
            {
                case BL_REPLY_MUX_REQSTATUS:
                {
                    MSG_signalInsert(frame, (Uint64_t)msg->reply.reqStatus.requestStatus, 0, 8);
                    MSG_signalInsert(frame, (Uint64_t)msg->reply.reqStatus.requestMux, 8, 8);
                    ret = OK;
                    break;
                }
                case BL_REPLY_MUX_VERSION:
                {
                    MSG_signalInsert(frame, (Uint64_t)msg->reply.version.bootloaderVersion, 0, 16);
                    MSG_signalInsert(frame, (Uint64_t)msg->reply.version.appVersion, 16, 16);
                    MSG_signalInsert(frame, (Uint64_t)msg->reply.version.hardwareVersion, 32, 16);
                    ret = OK;
                    break;
                }
                case BL_REPLY_MUX_SYSSTATUS:
                {
                    MSG_signalInsert(frame, (Uint64_t)msg->reply.sysStatus.imageCrc, 0, 32);
                    MSG_signalInsert(frame, (Uint64_t)msg->reply.sysStatus.appValid, 32, 1);
                    MSG_signalInsert(frame, (Uint64_t)msg->reply.sysStatus.errorReason, 40, 8);
                    ret = OK;
                    break;
                }
                default:
                {
                    ret = FAIL;
                    return ret;
                }
            }
            
            // Write the mux to the CAN frame
            MSG_signalInsert(frame, (Uint64_t)msg->reply.mux, 56, 8);
 
            // Write the CAN ID into the CAN frame
            frame->id = BL_IF_CAN_ID_REPLY;

            break;
        }

        case BL_IF_MESSAGE_REQUEST:
        {
            // Insert payload signals based on the mux
            switch(msg->request.mux)
            {
                case BL_REQUEST_MUX_UPDATE_INITIALIZE:
                {
                    MSG_signalInsert(frame, (Uint64_t)msg->request.updateInitialize.appSize, 0, 32);
                    MSG_signalInsert(frame, (Uint64_t)msg->request.updateInitialize.appVersion, 32, 16);
                    ret = OK;
                    break;
                }
                case BL_REQUEST_MUX_LOAD_CONFIGURE:
                {
                    MSG_signalInsert(frame, (Uint64_t)msg->request.loadConfigure.clusterAddress, 0, 32);
                    MSG_signalInsert(frame, (Uint64_t)msg->request.loadConfigure.clusterLength, 32, 16);
                    ret = OK;
                    break;
                }
                case BL_REQUEST_MUX_BUFFER_FLASH:
                {
                    MSG_signalInsert(frame, (Uint64_t)msg->request.bufferFlash.clusterCrc, 0, 32);
                    ret = OK;
                    break;
                }
                case BL_REQUEST_MUX_UPDATE_FINISH:
                {
                    MSG_signalInsert(frame, (Uint64_t)msg->request.updateFinish.appCrc, 0, 32);
                    ret = OK;
                    break;
                }
                case BL_REQUEST_MUX_APP_ERASE:
                case BL_REQUEST_MUX_APP_RUN:
                case BL_REQUEST_MUX_RESET:
                case BL_REQUEST_MUX_INFO:
                {
                    ret = OK;
                    break;
                }
                default:
                {
                    ret = FAIL;
                    return ret;
                }
            }

            // Write the mux to the CAN frame
            MSG_signalInsert(frame, (Uint64_t)msg->request.mux, 56, 8);
 
            // Write the CAN ID into the CAN frame
            frame->id = BL_IF_CAN_ID_REQUEST;

            break;
        }
        
        // Data message consists purely of up to 8 bytes of data
        case BL_IF_MESSAGE_DATA:
        {
            // Copy the payload into the CAN frame buffer
            // Note: CAN frame buffer holds values by byte,
            // while BL_if message data is by word
            // Cannot use memcpy here
            Uint16_t i = 0;
            for (i = 0; i < msg->lenBytes; i++)
            {
                Uint16_t shiftVal = (i % 2 == 0) ? 0 : 8;
                frame->buf[i] = (msg->data.dataWords[i / 2] >> shiftVal) & 0xFF;
            }

            // Write the CAN ID into the CAN frame
            frame->id = BL_IF_CAN_ID_DATA;

            ret = OK;
            break;
        }

        default:
        {
            ret = FAIL;
            return ret;
        }
    }

    // Write CAN buffer length
    frame->bufLen = msg->lenBytes;

    return ret;
}

/*
|===============================================================================
|
| Function:         F_canDecode
|
| Description:      Decodes a request or data message into a IfMessage_t struct
|
| Dependencies:
|
| Notes:            This function can decode any message type, however it is
|                   typically used to decode an incoming request or data message
|                   
|                   RAMFUNC attribute is installed as this function might be called
|                   if a CAN interrupt occurs during a flash modification operation
|
| Side Effects:
|
| Return Value:     OK or FAIL
|
|===============================================================================
| Variable     Access  Description  (I=input O=output I/O=in/out)
|-------------------------------------------------------------------------------
| msg          O       BL_REQUEST or BL_DATA message destination
| frame        I       CAN frame source
|=============================================================================*/

RAMFUNC static Uint16_t F_canDecode
(
    IfMessage_t *msg,
    const can_t *frame
)
{
    Uint16_t ret = FAIL;

    // CAN ID must be request or data type
    if ((msg == NULL) || (frame == NULL) || 
        ((frame->id != BL_IF_CAN_ID_REQUEST) && 
         (frame->id != BL_IF_CAN_ID_DATA) &&
         (frame->id != BL_IF_CAN_ID_REPLY)))
    {
        ret = FAIL;
        return ret;
    }

    switch (frame->id)
    {
        case BL_IF_CAN_ID_REQUEST:
        {
            msg->type = BL_IF_MESSAGE_REQUEST;

            // Decode the CAN frame based on the mux
            msg->request.mux= (IfMux_t)MSG_signalExtract(frame, 56, 8);

            switch (msg->request.mux)
            {
                case BL_REQUEST_MUX_UPDATE_INITIALIZE:
                {
                    msg->request.updateInitialize.appSize = (Uint32_t)MSG_signalExtract(frame, 0, 32);
                    msg->request.updateInitialize.appVersion = (Uint16_t)MSG_signalExtract(frame, 32, 16);
                    ret = OK;
                    break;
                }

                case BL_REQUEST_MUX_LOAD_CONFIGURE:
                {
                    msg->request.loadConfigure.clusterAddress = (Uint32_t)MSG_signalExtract(frame, 0, 32);
                    msg->request.loadConfigure.clusterLength = (Uint16_t)MSG_signalExtract(frame, 32, 16);
                    ret = OK;
                    break;
                }

                case BL_REQUEST_MUX_BUFFER_FLASH:
                {
                    msg->request.bufferFlash.clusterCrc = (Uint32_t)MSG_signalExtract(frame, 0, 32);
                    ret = OK;
                    break;
                }

                case BL_REQUEST_MUX_UPDATE_FINISH:
                {
                    msg->request.updateFinish.appCrc = (Uint32_t)MSG_signalExtract(frame, 0, 32);
                    ret = OK;
                    break;
                }

                case BL_REQUEST_MUX_APP_ERASE:
                case BL_REQUEST_MUX_APP_RUN:
                case BL_REQUEST_MUX_RESET:
                case BL_REQUEST_MUX_INFO:
                {
                    ret = OK;
                    break;
                }
                
                default:
                {
                    ret = FAIL;
                    return ret;
                }
            }

            break;
        }
        case BL_IF_CAN_ID_REPLY:
        {
            msg->type = BL_IF_MESSAGE_REPLY;

            // Decode the CAN frame based on the mux
            msg->reply.mux = (IfMux_t)MSG_signalExtract(frame, 56, 8);

            switch (msg->reply.mux)
            {
                case BL_REPLY_MUX_REQSTATUS:
                {
                    msg->reply.reqStatus.requestStatus = (IfRequestStatus_t)MSG_signalExtract(frame, 0, 8);
                    msg->reply.reqStatus.requestMux = (IfMux_t)MSG_signalExtract(frame, 8, 8);
                    ret = OK;
                    break;
                }

                case BL_REPLY_MUX_VERSION:
                {
                    msg->reply.version.bootloaderVersion = (Uint16_t)MSG_signalExtract(frame, 0, 16);
                    msg->reply.version.appVersion= (Uint16_t)MSG_signalExtract(frame, 16, 16);
                    msg->reply.version.hardwareVersion= (Uint16_t)MSG_signalExtract(frame, 32, 16);
                    ret = OK;
                    break;
                }

                case BL_REPLY_MUX_SYSSTATUS:
                {
                    msg->reply.sysStatus.imageCrc = (Uint32_t)MSG_signalExtract(frame, 0, 32);
                    msg->reply.sysStatus.appValid = (Bool_t)MSG_signalExtract(frame, 32, 1);
                    msg->reply.sysStatus.errorReason = (IfErrorReason_t)MSG_signalExtract(frame, 40, 8);
                    ret = OK;
                    break;
                }

                default:
                {
                    ret = FAIL;
                    break;
                }
            }

            break;
        }
        case BL_IF_CAN_ID_DATA:
        {
            msg->type = BL_IF_MESSAGE_DATA;
            // Copy the CAN frame buffer into the data message
            // Note: CAN frame buffer holds values by byte,
            // while BL_if message data is by word
            // Cannot use memcpy here
            
            Uint16_t i = 0;
            Uint16_t lenWords = (frame->bufLen + 1) / 2;
            for (i = 0; i < lenWords; i++)
            {
                msg->data.dataWords[i] = 0;
                msg->data.dataWords[i] |= (frame->buf[2 * i] & 0xFF);
                if (((2 * i) + 1) < frame->bufLen)
                {
                    msg->data.dataWords[i] |= ((frame->buf[(2 * i) + 1] << 8) & 0xFF00);
                }
            }
    
            ret = OK;
            break;
        }
        default:
        {
            ret = FAIL;
            break;
        }
    }
 
    // Write message length
    msg->lenBytes = frame->bufLen;

    return ret;
}

/*
|===============================================================================
|
| Function:         BL_ifInit
|
| Description:      Initializes the BL_if module
|
| Dependencies:     MSG and BCAN modules must be initialized first
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
|=============================================================================*/

void BL_ifInit
(
    void
)
{
    // Initialize the Rx queue for BL_if
    F_ifQueueInit(&F_ifRxQueue, F_ifRxQueueBuffer, BL_IF_RX_QUEUE_LENGTH);

    // Install the MSG Rx callback
    MSG_rxHandlerSet(BL_ifMsgRxHandler);
}

/*
|===============================================================================
|
| Function:         BL_ifTx
|
| Description:      Transmits a BL_if message
|
| Dependencies:
|
| Notes:            
|
| Side Effects:
|
| Return Value:     OK or FAIL, depending on whether message was transmitted
|
|===============================================================================
| Variable     Access  Description  (I=input O=output I/O=in/out)
|-------------------------------------------------------------------------------
| msg          I       BL_if message to be transmitted
|=============================================================================*/

Uint16_t BL_ifTx
(
    const IfMessage_t *msg
)
{
    Uint16_t ret = FAIL;
    can_t frame = {0};
    
    // Convert IfMessage_t type message into can_t type CAN frame
    ret = F_canEncode(&frame, msg);
    
    // Transmit CAN message using MSG API
    if (ret != FAIL)
    {
        ret = MSG_tx(&frame);
    }

    return (ret);
}

/*
|===============================================================================
|
| Function:         BL_ifRx
|
| Description:      Polls and receives a bootloader interface message
|
| Dependencies:     BL_ifInit, MSG_initialize, and BCAN_initialize must all
|                   have been called
|
| Notes:
|
| Side Effects:
|
| Return Value:     OK or FAIL
|
|===============================================================================
| Variable     Access  Description  (I=input O=output I/O=in/out)
|-------------------------------------------------------------------------------
| msg          O       Bootloader interface message destination
|=============================================================================*/

Uint16_t BL_ifRx
(
    IfMessage_t *msg
)
{
    Uint16_t ret = FAIL;

    if (F_ifQueueIsEmpty(&F_ifRxQueue) == FALSE)
    {
        F_ifQueuePop(&F_ifRxQueue, msg);
        ret = OK;
    }
    else 
    {
        ret = FAIL;
    }

    return ret;
}


/*
|===============================================================================
|
| Function:         BL_ifMsgRxHandler
|
| Description:      Handler function to be installed into MSG Rx Callback
|
| Dependencies:
|
| Notes:            RAMFUNC attribute is installed as this function might be called
|                   during a flash modification operation
|
| Side Effects:
|
| Return Value:     TRUE or FALSE, identifying whether the received CAN frame 
|                   is intended for the bootloader
|
|===============================================================================
| Variable     Access  Description  (I=input O=output I/O=in/out)
|-------------------------------------------------------------------------------
| frame        I       Incoming CAN frame from MSG module
|=============================================================================*/
RAMFUNC Bool_t BL_ifMsgRxHandler
(
    const can_t *frame
)
{
    IfMessage_t rxMsg;
    Bool_t isValidIfMsg = FALSE;

    if (F_ifQueueIsFull(&F_ifRxQueue) == FALSE)
    {
        isValidIfMsg = ((frame->id == BL_IF_CAN_ID_REQUEST) ||
                        (frame->id == BL_IF_CAN_ID_DATA));

        if (isValidIfMsg)
        {
            // Decode the CAN frame and convert it into type IfMessage_t
            F_canDecode(&rxMsg, frame);

            // Push into BL_if message queue
            F_ifQueuePush(&F_ifRxQueue, &rxMsg);
        }
    }

    return isValidIfMsg;
}

/*
|===============================================================================
|
| Function:         BL_ifReplyReqStatusTx
|
| Description:
|
| Dependencies:     Transmits a BL_REPLY_REQSTATUS message
|
| Notes:
|
| Side Effects:
|
| Return Value:
|
|===============================================================================
| Variable       Access  Description  (I=input O=output I/O=in/out)
|-------------------------------------------------------------------------------
| requestStatus  I       Status of the request
| requestMux     I       Multiplexor value of the associated request message
|=============================================================================*/

Uint16_t BL_ifReplyReqStatusTx
(
    IfRequestStatus_t requestStatus,
    IfMux_t requestMux
)
{
    IfMessage_t msg;
    msg.type = BL_IF_MESSAGE_REPLY;
    msg.reply.mux = BL_REPLY_MUX_REQSTATUS;
    msg.reply.reqStatus.requestStatus = requestStatus;
    msg.reply.reqStatus.requestMux = requestMux;
    msg.lenBytes = BL_IF_CAN_LEN_REPLY;
    
    return BL_ifTx(&msg);
}

/*
|===============================================================================
|
| Function:         BL_ifReplyVersionTx
|
| Description:      Transmits a BL_REPLY_VERSION message
|
| Dependencies:
|
| Notes:
|
| Side Effects:
|
| Return Value:     OK or FAIL
|
|===============================================================================
| Variable           Access  Description  (I=input O=output I/O=in/out)
|-------------------------------------------------------------------------------
| bootloaderVersion  I       Bootloader version
| appVersion         I       Application version
| hardwareVersion    I       Hardware version
|=============================================================================*/

Uint16_t BL_ifReplyVersionTx
(
    Uint16_t bootloaderVersion,
    Uint16_t appVersion,
    Uint16_t hardwareVersion
)
{
    IfMessage_t msg;
    msg.type = BL_IF_MESSAGE_REPLY;
    msg.reply.mux = BL_REPLY_MUX_VERSION;
    msg.reply.version.bootloaderVersion = bootloaderVersion;
    msg.reply.version.appVersion = appVersion;
    msg.reply.version.hardwareVersion = hardwareVersion;
    msg.lenBytes = BL_IF_CAN_LEN_REPLY;
    
    return BL_ifTx(&msg);
}

/*
|===============================================================================
|
| Function:         BL_ifReplySysStatusTx
|
| Description:      Transmits a BL_REPLY_SYSSTATUS message
|
| Dependencies:
|
| Notes:
|
| Side Effects:
|
| Return Value:     OK or FAIL
|
|===============================================================================
| Variable      Access  Description  (I=input O=output I/O=in/out)
|-------------------------------------------------------------------------------
| imageCrc      I       Firmware image CRC-32 value
| appValid      I       Boolean telling whether app is valid or not
| errorReason   I       Enum denoting any potential error
|=============================================================================*/

Uint16_t BL_ifReplySysStatusTx
(
    Uint32_t        imageCrc,
    Bool_t          appValid,
    IfErrorReason_t errorReason
)
{
    IfMessage_t msg;
    msg.type = BL_IF_MESSAGE_REPLY;
    msg.reply.mux = BL_REPLY_MUX_SYSSTATUS;
    msg.reply.sysStatus.imageCrc = imageCrc;
    msg.reply.sysStatus.appValid = appValid;
    msg.reply.sysStatus.errorReason = errorReason;
    msg.lenBytes = BL_IF_CAN_LEN_REPLY;
    
    return BL_ifTx(&msg);
}

/*=== End of File ============================================================*/
