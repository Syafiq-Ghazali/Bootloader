/*
 * Bootloader CAN protocol interface.
 */

#include <string.h>
#include "BL_if.h"
#include "BRD_CAN.h"

static Uint16_t F_canEncode     (CanMsg_t *frame, const IfMessage_t *msg);
static Uint16_t F_canDecode     (IfMessage_t *msg, const CanMsg_t *frame);
static void     F_frameClear    (CanMsg_t *frame);
static void     F_writeU8       (CanMsg_t *frame, Uint16_t index, Uint16_t value);
static void     F_writeU16      (CanMsg_t *frame, Uint16_t index, Uint16_t value);
static void     F_writeU32      (CanMsg_t *frame, Uint16_t index, Uint32_t value);
static Uint16_t F_readU8        (const CanMsg_t *frame, Uint16_t index);
static Uint16_t F_readU16       (const CanMsg_t *frame, Uint16_t index);
static Uint32_t F_readU32       (const CanMsg_t *frame, Uint16_t index);

static CanHandle_t *F_canHandle = &BRD_CAN_HANDLE[CAN_INSTANCE_1];

void BL_ifInit
(
    void
)
{
    F_canHandle = &BRD_CAN_HANDLE[CAN_INSTANCE_1];
}

Uint16_t BL_ifTx
(
    const IfMessage_t *msg
)
{
    CanMsg_t frame = {0};

    if ((F_canEncode(&frame, msg) != OK) ||
        (BCAN_tx(F_canHandle, &frame) != CAN_STATUS_TX_OK))
    {
        return FAIL;
    }

    return OK;
}

Uint16_t BL_ifRx
(
    IfMessage_t *msg
)
{
    CanMsg_t frame = {0};

    if ((msg == NULL) || (F_canHandle == NULL))
    {
        return FAIL;
    }

    while (BCAN_rx(F_canHandle, &frame) == CAN_STATUS_RX_OK)
    {
        if (F_canDecode(msg, &frame) == OK)
        {
            return OK;
        }
    }

    return FAIL;
}

Bool_t BL_ifTxPending
(
    void
)
{
    return BCAN_txPending(F_canHandle);
}

Uint16_t BL_ifReplyReqStatusTx
(
    IfRequestStatus_t requestStatus,
    IfMux_t requestMux
)
{
    IfMessage_t msg;

    (void)memset(&msg, 0, sizeof(msg));

    msg.type = BL_IF_MESSAGE_REPLY;
    msg.reply.mux = BL_REPLY_MUX_REQSTATUS;
    msg.reply.requestStatus = requestStatus;
    msg.reply.requestMux = requestMux;
    msg.lenBytes = BL_IF_CAN_LEN_REPLY;

    return BL_ifTx(&msg);
}

Uint16_t BL_ifReplyVersionTx
(
    Uint16_t bootloaderVersion,
    Uint16_t appVersion,
    Uint16_t hardwareVersion
)
{
    IfMessage_t msg;

    (void)memset(&msg, 0, sizeof(msg));

    msg.type = BL_IF_MESSAGE_REPLY;
    msg.reply.mux = BL_REPLY_MUX_VERSION;
    msg.reply.bootloaderVersion = bootloaderVersion;
    msg.reply.appVersion = appVersion;
    msg.reply.hardwareVersion = hardwareVersion;
    msg.lenBytes = BL_IF_CAN_LEN_REPLY;

    return BL_ifTx(&msg);
}

Uint16_t BL_ifReplySysStatusTx
(
    Uint32_t imageCrc,
    Bool_t appValid,
    IfErrorReason_t errorReason
)
{
    IfMessage_t msg;

    (void)memset(&msg, 0, sizeof(msg));

    msg.type = BL_IF_MESSAGE_REPLY;
    msg.reply.mux = BL_REPLY_MUX_SYSSTATUS;
    msg.reply.imageCrc = imageCrc;
    msg.reply.appValid = appValid;
    msg.reply.errorReason = errorReason;
    msg.lenBytes = BL_IF_CAN_LEN_REPLY;

    return BL_ifTx(&msg);
}

static Uint16_t F_canEncode
(
    CanMsg_t *frame,
    const IfMessage_t *msg
)
{
    Uint16_t i;

    if ((frame == NULL) || (msg == NULL))
    {
        return FAIL;
    }

    F_frameClear(frame);

    switch (msg->type)
    {
        case BL_IF_MESSAGE_REPLY:
        {
            frame->id = BL_IF_CAN_ID_REPLY;
            frame->bufLen = BL_IF_CAN_LEN_REPLY;

            switch (msg->reply.mux)
            {
                case BL_REPLY_MUX_REQSTATUS:
                {
                    F_writeU8(frame, 0u, (Uint16_t)msg->reply.requestStatus);
                    F_writeU8(frame, 1u, (Uint16_t)msg->reply.requestMux);
                    break;
                }
                case BL_REPLY_MUX_VERSION:
                {
                    F_writeU16(frame, 0u, msg->reply.bootloaderVersion);
                    F_writeU16(frame, 2u, msg->reply.appVersion);
                    F_writeU16(frame, 4u, msg->reply.hardwareVersion);
                    break;
                }
                case BL_REPLY_MUX_SYSSTATUS:
                {
                    F_writeU32(frame, 0u, msg->reply.imageCrc);
                    F_writeU8(frame, 4u, (Uint16_t)msg->reply.appValid);
                    F_writeU8(frame, 5u, (Uint16_t)msg->reply.errorReason);
                    break;
                }
                default:
                {
                    return FAIL;
                }
            }

            F_writeU8(frame, 7u, (Uint16_t)msg->reply.mux);
            break;
        }

        case BL_IF_MESSAGE_REQUEST:
        {
            frame->id = BL_IF_CAN_ID_REQUEST;
            frame->bufLen = BL_IF_CAN_LEN_REQUEST;

            switch (msg->request.mux)
            {
                case BL_REQUEST_MUX_UPDATE_INITIALIZE:
                {
                    F_writeU32(frame, 0u, msg->request.appSize);
                    F_writeU16(frame, 4u, msg->request.appVersion);
                    break;
                }
                case BL_REQUEST_MUX_LOAD_CONFIGURE:
                {
                    F_writeU32(frame, 0u, msg->request.clusterAddress);
                    F_writeU16(frame, 4u, msg->request.clusterLength);
                    break;
                }
                case BL_REQUEST_MUX_BUFFER_FLASH:
                {
                    F_writeU32(frame, 0u, msg->request.clusterCrc);
                    break;
                }
                case BL_REQUEST_MUX_UPDATE_FINISH:
                {
                    F_writeU32(frame, 0u, msg->request.appCrc);
                    break;
                }
                case BL_REQUEST_MUX_APP_ERASE:
                case BL_REQUEST_MUX_APP_RUN:
                case BL_REQUEST_MUX_RESET:
                case BL_REQUEST_MUX_INFO:
                {
                    break;
                }
                default:
                {
                    return FAIL;
                }
            }

            F_writeU8(frame, 7u, (Uint16_t)msg->request.mux);
            break;
        }

        case BL_IF_MESSAGE_DATA:
        {
            frame->id = BL_IF_CAN_ID_DATA;
            frame->bufLen = MIN(msg->lenBytes, BL_IF_CAN_LEN_DATA);

            for (i = 0u; i < frame->bufLen; i++)
            {
                Uint16_t shift = ((i & 1u) == 0u) ? 0u : 8u;
                frame->buf[i] = (msg->data.dataWords[i / 2u] >> shift) & 0xFFu;
            }

            break;
        }

        default:
        {
            return FAIL;
        }
    }

    return OK;
}

static Uint16_t F_canDecode
(
    IfMessage_t *msg,
    const CanMsg_t *frame
)
{
    Uint16_t i;
    Uint16_t lenWords;

    if ((msg == NULL) || (frame == NULL))
    {
        return FAIL;
    }

    (void)memset(msg, 0, sizeof(IfMessage_t));
    msg->lenBytes = MIN(frame->bufLen, BL_IF_CAN_LEN);

    switch (frame->id)
    {
        case BL_IF_CAN_ID_REQUEST:
        {
            msg->type = BL_IF_MESSAGE_REQUEST;
            msg->request.mux = (IfMux_t)F_readU8(frame, 7u);

            switch (msg->request.mux)
            {
                case BL_REQUEST_MUX_UPDATE_INITIALIZE:
                {
                    msg->request.appSize = F_readU32(frame, 0u);
                    msg->request.appVersion = F_readU16(frame, 4u);
                    break;
                }
                case BL_REQUEST_MUX_LOAD_CONFIGURE:
                {
                    msg->request.clusterAddress = F_readU32(frame, 0u);
                    msg->request.clusterLength = F_readU16(frame, 4u);
                    break;
                }
                case BL_REQUEST_MUX_BUFFER_FLASH:
                {
                    msg->request.clusterCrc = F_readU32(frame, 0u);
                    break;
                }
                case BL_REQUEST_MUX_UPDATE_FINISH:
                {
                    msg->request.appCrc = F_readU32(frame, 0u);
                    break;
                }
                case BL_REQUEST_MUX_APP_ERASE:
                case BL_REQUEST_MUX_APP_RUN:
                case BL_REQUEST_MUX_RESET:
                case BL_REQUEST_MUX_INFO:
                {
                    break;
                }
                default:
                {
                    return FAIL;
                }
            }

            break;
        }

        case BL_IF_CAN_ID_REPLY:
        {
            msg->type = BL_IF_MESSAGE_REPLY;
            msg->reply.mux = (IfMux_t)F_readU8(frame, 7u);

            switch (msg->reply.mux)
            {
                case BL_REPLY_MUX_REQSTATUS:
                {
                    msg->reply.requestStatus = (IfRequestStatus_t)F_readU8(frame, 0u);
                    msg->reply.requestMux = (IfMux_t)F_readU8(frame, 1u);
                    break;
                }
                case BL_REPLY_MUX_VERSION:
                {
                    msg->reply.bootloaderVersion = F_readU16(frame, 0u);
                    msg->reply.appVersion = F_readU16(frame, 2u);
                    msg->reply.hardwareVersion = F_readU16(frame, 4u);
                    break;
                }
                case BL_REPLY_MUX_SYSSTATUS:
                {
                    msg->reply.imageCrc = F_readU32(frame, 0u);
                    msg->reply.appValid = (Bool_t)F_readU8(frame, 4u);
                    msg->reply.errorReason = (IfErrorReason_t)F_readU8(frame, 5u);
                    break;
                }
                default:
                {
                    return FAIL;
                }
            }

            break;
        }

        case BL_IF_CAN_ID_DATA:
        {
            msg->type = BL_IF_MESSAGE_DATA;
            lenWords = (msg->lenBytes + 1u) / 2u;

            for (i = 0u; i < lenWords; i++)
            {
                msg->data.dataWords[i] = F_readU16(frame, (Uint16_t)(2u * i));
            }

            break;
        }

        default:
        {
            return FAIL;
        }
    }

    return OK;
}

static void F_frameClear
(
    CanMsg_t *frame
)
{
    Uint16_t i;

    frame->id = 0u;
    frame->bufLen = BL_IF_CAN_LEN;

    for (i = 0u; i < BL_IF_CAN_LEN; i++)
    {
        frame->buf[i] = 0u;
    }
}

static void F_writeU8
(
    CanMsg_t *frame,
    Uint16_t index,
    Uint16_t value
)
{
    if (index < BL_IF_CAN_LEN)
    {
        frame->buf[index] = value & 0xFFu;
    }
}

static void F_writeU16
(
    CanMsg_t *frame,
    Uint16_t index,
    Uint16_t value
)
{
    F_writeU8(frame, index, value);
    F_writeU8(frame, (Uint16_t)(index + 1u), value >> 8u);
}

static void F_writeU32
(
    CanMsg_t *frame,
    Uint16_t index,
    Uint32_t value
)
{
    F_writeU8(frame, index, (Uint16_t)value);
    F_writeU8(frame, (Uint16_t)(index + 1u), (Uint16_t)(value >> 8u));
    F_writeU8(frame, (Uint16_t)(index + 2u), (Uint16_t)(value >> 16u));
    F_writeU8(frame, (Uint16_t)(index + 3u), (Uint16_t)(value >> 24u));
}

static Uint16_t F_readU8
(
    const CanMsg_t *frame,
    Uint16_t index
)
{
    return (index < BL_IF_CAN_LEN) ? (frame->buf[index] & 0xFFu) : 0u;
}

static Uint16_t F_readU16
(
    const CanMsg_t *frame,
    Uint16_t index
)
{
    Uint16_t value = F_readU8(frame, index);
    value |= (Uint16_t)(F_readU8(frame, (Uint16_t)(index + 1u)) << 8u);
    return value;
}

static Uint32_t F_readU32
(
    const CanMsg_t *frame,
    Uint16_t index
)
{
    Uint32_t value = F_readU8(frame, index);
    value |= ((Uint32_t)F_readU8(frame, (Uint16_t)(index + 1u))) << 8u;
    value |= ((Uint32_t)F_readU8(frame, (Uint16_t)(index + 2u))) << 16u;
    value |= ((Uint32_t)F_readU8(frame, (Uint16_t)(index + 3u))) << 24u;
    return value;
}
