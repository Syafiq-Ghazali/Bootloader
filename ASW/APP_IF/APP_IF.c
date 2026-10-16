/* Application-flash commands. CAN framing belongs to BL_if; lifecycle to BL_sm. */
#include <string.h>
#include "APP_IF.h"

static Uint16_t F_tag(const CtxData_t *ctx)
{
    return ctx->currMsg.type == BL_IF_MESSAGE_REQUEST ? ctx->currMsg.request.tag :
           ctx->currMsg.type == BL_IF_MESSAGE_SEQUENCED_DATA ? ctx->currMsg.data.tag : 0u;
}

static Uint16_t F_send(CtxData_t *ctx, IfReplyMessage_t *reply)
{
    reply->tag = F_tag(ctx);
    if (BL_ifReplyTx(reply) != OK)
    {
        ctx->errorReason = BL_ERROR_TX_TIMEOUT;
        return FAIL;
    }
    return OK;
}

static Uint16_t F_ack(CtxData_t *ctx, IfRequestStatus_t status, IfErrorReason_t error)
{
    IfReplyMessage_t reply;
    memset(&reply, 0, sizeof(reply));
    reply.mux = BL_REPLY_MUX_REQSTATUS;
    if (ctx->currMsg.type == BL_IF_MESSAGE_REQUEST) reply.requestMux = ctx->currMsg.request.mux;
    else reply.requestMux = BL_REQUEST_MUX_DATA;
    reply.requestStatus = status;
    reply.errorReason = error;
    reply.offset = ctx->clstrWordsLoaded;
    if (error != BL_ERROR_NONE) ctx->errorReason = error;
    return F_send(ctx, &reply);
}

static void F_clusterClear(CtxData_t *ctx)
{
    BL_clstrInit();
    ctx->clstrReady = FALSE;
    ctx->clstrCommitted = FALSE;
    ctx->clstrWordsLoaded = 0u;
    ctx->lastDataCount = 0u;
}

void APP_IF_imageRefresh(CtxData_t *ctx)
{
    Uint32_t computed;
    BL_imgVariableMetadataRead(&ctx->vMetadata);
    computed = BL_imgCrcCompute();
    ctx->appValid = (ctx->vMetadata.appSizeWords != 0uL) &&
                    (ctx->vMetadata.appSizeWords <= BL_IMG_APPLICATION_REGION_SIZE_WORDS) &&
                    (computed == ctx->vMetadata.appCrc);
    ctx->vMetadata.appCrc = computed;
}

static void F_data(CtxData_t *ctx, SmState_t state)
{
    IfDataMessage_t *data = &ctx->currMsg.data;
    Bool_t sequenced = ctx->currMsg.type == BL_IF_MESSAGE_SEQUENCED_DATA;
    Uint16_t count = sequenced ? data->wordCount : ctx->currMsg.lenBytes / 2u;
    if ((state != BL_STATE_UPDATE) || !ctx->clstrReady)
    {
        F_ack(ctx, BL_REQUEST_STATUS_INVALID, BL_ERROR_STATE);
        return;
    }
    if ((count == 0u) || (count > (sequenced ? BL_IF_SEQ_DATA_WORDS : BL_IF_DATA_LEN_WORDS)) ||
        (!sequenced && ((ctx->currMsg.lenBytes & 1u) != 0u)))
    {
        F_ack(ctx, BL_REQUEST_STATUS_INVALID, BL_ERROR_LENGTH);
        return;
    }
    if (sequenced)
    {
        if (data->tag != ctx->clstrTag)
        {
            F_ack(ctx, BL_REQUEST_STATUS_RETRY, BL_ERROR_SEQUENCE);
            return;
        }
        // A retry after a lost DATA acknowledgement must not append data twice.
        if ((ctx->lastDataCount == count) && (ctx->lastDataOffset == data->offset) &&
            (memcmp(ctx->lastDataWords, data->dataWords, count * sizeof(Uint16_t)) == 0))
        {
            F_ack(ctx, BL_REQUEST_STATUS_COMPLETE, BL_ERROR_NONE);
            return;
        }
        if (data->offset != ctx->clstrWordsLoaded)
        {
            F_ack(ctx, BL_REQUEST_STATUS_RETRY, BL_ERROR_SEQUENCE);
            return;
        }
    }
    if ((count > ctx->clstr.length - ctx->clstrWordsLoaded) ||
        (BL_clstrLoad(&ctx->clstr, data->dataWords, count) != OK))
    {
        F_ack(ctx, BL_REQUEST_STATUS_INVALID, BL_ERROR_LENGTH);
        return;
    }
    if (sequenced)
    {
        ctx->lastDataOffset = data->offset;
        ctx->lastDataCount = count;
        memcpy(ctx->lastDataWords, data->dataWords, count * sizeof(Uint16_t));
    }
    else ctx->lastDataCount = 0u;
    ctx->clstrWordsLoaded += count;
    F_ack(ctx, BL_REQUEST_STATUS_COMPLETE, BL_ERROR_NONE);
}

SmState_t APP_IF_process(CtxData_t *ctx, SmState_t state)
{
    IfRequestMessage_t *request = &ctx->currMsg.request;
    IfReplyMessage_t reply;
    memset(&reply, 0, sizeof(reply));
    if ((ctx->currMsg.type == BL_IF_MESSAGE_DATA) ||
        (ctx->currMsg.type == BL_IF_MESSAGE_SEQUENCED_DATA))
    {
        F_data(ctx, state);
        return state;
    }
    if (ctx->currMsg.type != BL_IF_MESSAGE_REQUEST) return state;

    // These queries/control commands are available in both MENU and UPDATE.
    switch (request->mux)
    {
        case BL_REQUEST_MUX_INFO:
            reply.mux = BL_REPLY_MUX_VERSION;
            reply.bootloaderVersion = ctx->fMetadata.bootloaderVersion;
            reply.appVersion = ctx->vMetadata.appVersion;
            reply.hardwareVersion = ctx->fMetadata.hardwareVersion;
            if (F_send(ctx, &reply) != OK) return state;
            reply.mux = BL_REPLY_MUX_CAPABILITIES;
            reply.hardwareSerialNumber = ctx->fMetadata.hardwareSerialNumber;
            if (F_send(ctx, &reply) != OK) return state;
            reply.mux = BL_REPLY_MUX_SYSSTATUS;
            reply.imageCrc = ctx->vMetadata.appCrc;
            reply.appValid = ctx->appValid;
            reply.errorReason = ctx->errorReason;
            if (F_send(ctx, &reply) == OK) F_ack(ctx, BL_REQUEST_STATUS_COMPLETE, BL_ERROR_NONE);
            return state;

        case BL_REQUEST_MUX_STATUS:
            reply.mux = BL_REPLY_MUX_STATE;
            reply.state = (Uint16_t)state;
            reply.clusterReady = ctx->clstrReady;
            reply.wordsWritten = ctx->wordsWritten;
            if (F_send(ctx, &reply) != OK) return state;
            reply.mux = BL_REPLY_MUX_SYSSTATUS;
            reply.imageCrc = ctx->vMetadata.appCrc;
            reply.appValid = ctx->appValid;
            reply.errorReason = ctx->errorReason;
            if (F_send(ctx, &reply) == OK) F_ack(ctx, BL_REQUEST_STATUS_COMPLETE, BL_ERROR_NONE);
            return state;

        case BL_REQUEST_MUX_MEMORY_MAP:
            reply.mux = BL_REPLY_MUX_MEMORY_START;
            reply.address = BL_IMG_APPLICATION_BEGIN_ADDR;
            reply.offset = (Uint16_t)BFLASH_SECTOR_SIZE_WORDS;
            if (F_send(ctx, &reply) != OK) return state;
            reply.mux = BL_REPLY_MUX_MEMORY_SIZE;
            reply.length = BL_IMG_APPLICATION_REGION_SIZE_WORDS;
            reply.offset = (Uint16_t)BL_BLOCK_BUFFER_LENGTH;
            if (F_send(ctx, &reply) == OK) F_ack(ctx, BL_REQUEST_STATUS_COMPLETE, BL_ERROR_NONE);
            return state;

        case BL_REQUEST_MUX_KEEP_ALIVE:
            F_ack(ctx, BL_REQUEST_STATUS_COMPLETE, BL_ERROR_NONE);
            return state;

        case BL_REQUEST_MUX_RESET:
            return F_ack(ctx, BL_REQUEST_STATUS_COMPLETE, BL_ERROR_NONE) == OK ? BL_STATE_RESET : state;

        case BL_REQUEST_MUX_READ:
        {
            Uint16_t offset;
            if ((request->length == 0u) || (request->length > APP_IF_MAX_READ_WORDS))
                F_ack(ctx, BL_REQUEST_STATUS_INVALID, BL_ERROR_LENGTH);
            else if (!BL_imgRangeValid(request->address, request->length))
                F_ack(ctx, BL_REQUEST_STATUS_INVALID, BL_ERROR_ADDRESS);
            else
            {
                if (F_ack(ctx, BL_REQUEST_STATUS_IN_PROGRESS, BL_ERROR_NONE) != OK) return state;
                reply.mux = BL_REPLY_MUX_READ_DATA;
                for (offset = 0u; offset < request->length; offset += BL_IF_SEQ_DATA_WORDS)
                {
                    Uint16_t remaining = request->length - offset;
                    Uint16_t count = remaining < BL_IF_SEQ_DATA_WORDS ? remaining : BL_IF_SEQ_DATA_WORDS;
                    reply.offset = offset;
                    reply.words[1] = 0xFFFFu;
                    if (BL_imgRead(request->address + offset, reply.words, count) != OK)
                    {
                        F_ack(ctx, BL_REQUEST_STATUS_FAILED, BL_ERROR_ADDRESS);
                        return state;
                    }
                    if (F_send(ctx, &reply) != OK) return state;
                }
                F_ack(ctx, BL_REQUEST_STATUS_COMPLETE, BL_ERROR_NONE);
            }
            return state;
        }

        case BL_REQUEST_MUX_VERIFY:
        {
            Uint32_t address = request->address;
            Uint32_t length = request->length;
            if ((address == 0uL) && (length == 0uL))
            {
                address = BL_IMG_APPLICATION_BEGIN_ADDR;
                length = BL_IMG_APPLICATION_REGION_SIZE_WORDS;
            }
            if (!BL_imgRangeValid(address, length))
                F_ack(ctx, BL_REQUEST_STATUS_INVALID, BL_ERROR_ADDRESS);
            else if (F_ack(ctx, BL_REQUEST_STATUS_IN_PROGRESS, BL_ERROR_NONE) == OK)
            {
                reply.mux = BL_REPLY_MUX_CRC;
                if (BL_imgRangeCrcCompute(address, length, &reply.imageCrc) != OK)
                    F_ack(ctx, BL_REQUEST_STATUS_FAILED, BL_ERROR_ADDRESS);
                else if (F_send(ctx, &reply) == OK)
                    F_ack(ctx, BL_REQUEST_STATUS_COMPLETE, BL_ERROR_NONE);
            }
            return state;
        }
        default: break;
    }

    if (state == BL_STATE_MENU)
    {
        switch (request->mux)
        {
            case BL_REQUEST_MUX_APP_ERASE:
            case BL_REQUEST_MUX_ERASE_RANGE:
            {
                Uint32_t address = request->mux == BL_REQUEST_MUX_APP_ERASE ?
                                   BL_IMG_APPLICATION_BEGIN_ADDR : request->address;
                Uint32_t length = request->mux == BL_REQUEST_MUX_APP_ERASE ?
                                  BL_IMG_APPLICATION_REGION_SIZE_WORDS : request->length;
                if (!BL_imgRangeValid(address, length))
                    F_ack(ctx, BL_REQUEST_STATUS_INVALID, BL_ERROR_ADDRESS);
                else if (((address % BFLASH_SECTOR_SIZE_WORDS) != 0uL) ||
                         ((length % BFLASH_SECTOR_SIZE_WORDS) != 0uL))
                    F_ack(ctx, BL_REQUEST_STATUS_INVALID, BL_ERROR_ALIGNMENT);
                else
                {
                    if (F_ack(ctx, BL_REQUEST_STATUS_IN_PROGRESS, BL_ERROR_NONE) != OK) return state;
                    ctx->appValid = FALSE;
                    if (BL_imgRangeErase(address, length) != OK)
                    {
                        F_ack(ctx, BL_REQUEST_STATUS_FAILED, BL_ERROR_FLASH);
                        return BL_STATE_ERROR;
                    }
                    ctx->vMetadata.appCrc = BL_imgCrcCompute();
                    F_ack(ctx, BL_REQUEST_STATUS_COMPLETE, BL_ERROR_NONE);
                }
                return state;
            }
            case BL_REQUEST_MUX_UPDATE_INITIALIZE:
                if ((request->appSize == 0uL) || (request->appSize > BL_IMG_APPLICATION_REGION_SIZE_WORDS))
                    F_ack(ctx, BL_REQUEST_STATUS_INVALID, BL_ERROR_APP_SIZE);
                else
                {
                    F_clusterClear(ctx);
                    ctx->vMetadata.appVersion = request->appVersion;
                    ctx->vMetadata.appSizeWords = request->appSize;
                    ctx->appValid = FALSE;
                    ctx->clstrCrcRetryCount = 0u;
                    ctx->wordsWritten = 0uL;
                    ctx->errorReason = BL_ERROR_NONE;
                    F_ack(ctx, BL_REQUEST_STATUS_COMPLETE, BL_ERROR_NONE);
                    return BL_STATE_UPDATE;
                }
                return state;
            case BL_REQUEST_MUX_APP_RUN:
                // Recheck flash and saved metadata immediately before allowing a jump.
                APP_IF_imageRefresh(ctx);
                if (!ctx->appValid) F_ack(ctx, BL_REQUEST_STATUS_FAILED, BL_ERROR_APP_CRC);
                else if (F_ack(ctx, BL_REQUEST_STATUS_COMPLETE, BL_ERROR_NONE) == OK) return BL_STATE_APP_RUN;
                return state;
            default:
                F_ack(ctx, BL_REQUEST_STATUS_INVALID, BL_ERROR_STATE);
                return state;
        }
    }

    switch (request->mux)
    {
        case BL_REQUEST_MUX_LOAD_CONFIGURE:
            if (!BL_imgRangeValid(request->clusterAddress, request->clusterLength))
                F_ack(ctx, BL_REQUEST_STATUS_INVALID, BL_ERROR_ADDRESS);
            else if (((request->clusterAddress % APP_IF_WRITE_ALIGNMENT_WORDS) != 0uL) ||
                     ((request->clusterLength % APP_IF_WRITE_ALIGNMENT_WORDS) != 0u))
                F_ack(ctx, BL_REQUEST_STATUS_INVALID, BL_ERROR_ALIGNMENT);
            else if (request->clusterLength > BL_BLOCK_BUFFER_LENGTH)
                F_ack(ctx, BL_REQUEST_STATUS_INVALID, BL_ERROR_LENGTH);
            else if ((ctx->clstrReady || ctx->clstrCommitted) && (request->tag == ctx->clstrTag))
            {
                // Retransmitting the same BEGIN preserves already acknowledged data.
                if ((request->clusterAddress == ctx->clstr.address) && (request->clusterLength == ctx->clstr.length))
                    F_ack(ctx, BL_REQUEST_STATUS_COMPLETE, BL_ERROR_NONE);
                else F_ack(ctx, BL_REQUEST_STATUS_INVALID, BL_ERROR_SEQUENCE);
            }
            else
            {
                F_clusterClear(ctx);
                ctx->clstr.address = request->clusterAddress;
                ctx->clstr.length = request->clusterLength;
                ctx->clstrTag = request->tag;
                if (BL_clstrConfig(&ctx->clstr) != OK)
                    F_ack(ctx, BL_REQUEST_STATUS_FAILED, BL_ERROR_LOAD_CONFIGURE);
                else
                {
                    ctx->clstrReady = TRUE;
                    F_ack(ctx, BL_REQUEST_STATUS_COMPLETE, BL_ERROR_NONE);
                }
            }
            return state;

        case BL_REQUEST_MUX_BUFFER_FLASH:
            if (request->tag != ctx->clstrTag)
                F_ack(ctx, BL_REQUEST_STATUS_RETRY, BL_ERROR_SEQUENCE);
            else if (ctx->clstrCommitted && (request->clusterCrc == ctx->clstr.crc))
                F_ack(ctx, BL_REQUEST_STATUS_COMPLETE, BL_ERROR_NONE);
            else if (!ctx->clstrReady || (ctx->clstrWordsLoaded != ctx->clstr.length))
                F_ack(ctx, BL_REQUEST_STATUS_INVALID, BL_ERROR_LENGTH);
            else
            {
                BL_clstrCrcCompute(&ctx->clstr);
                if (ctx->clstr.crc != request->clusterCrc)
                {
                    if (ctx->clstrCrcRetryCount++ < BL_CLUSTER_CRC_MAX_RETRIES)
                        F_ack(ctx, BL_REQUEST_STATUS_RETRY, BL_ERROR_CLUSTER_CRC);
                    else
                    {
                        F_ack(ctx, BL_REQUEST_STATUS_FAILED, BL_ERROR_CLUSTER_CRC);
                        return BL_STATE_ERROR;
                    }
                }
                else
                {
                    if (F_ack(ctx, BL_REQUEST_STATUS_IN_PROGRESS, BL_ERROR_NONE) != OK) return state;
                    if (BL_clstrFlash() != OK)
                    {
                        F_ack(ctx, BL_REQUEST_STATUS_FAILED, BL_ERROR_FLASH);
                        return BL_STATE_ERROR;
                    }
                    ctx->clstrReady = FALSE;
                    ctx->clstrCommitted = TRUE;
                    ctx->wordsWritten += ctx->clstr.length;
                    ctx->clstrCrcRetryCount = 0u;
                    F_ack(ctx, BL_REQUEST_STATUS_COMPLETE, BL_ERROR_NONE);
                }
            }
            return state;

        case BL_REQUEST_MUX_UPDATE_FINISH:
            if (ctx->clstrReady)
            {
                F_ack(ctx, BL_REQUEST_STATUS_INVALID, BL_ERROR_STATE);
                return state;
            }
            if (F_ack(ctx, BL_REQUEST_STATUS_IN_PROGRESS, BL_ERROR_NONE) != OK) return state;
            ctx->vMetadata.appCrc = BL_imgCrcCompute();
            if (ctx->vMetadata.appCrc != request->appCrc)
            {
                ctx->appValid = FALSE;
                F_ack(ctx, BL_REQUEST_STATUS_FAILED, BL_ERROR_APP_CRC);
                return BL_STATE_ERROR;
            }
            if ((BL_imgVariableMetadataErase() != OK) ||
                (BL_imgVariableMetadataWrite(&ctx->vMetadata) != OK))
            {
                F_ack(ctx, BL_REQUEST_STATUS_FAILED, BL_ERROR_FLASH);
                return BL_STATE_ERROR;
            }
            ctx->appValid = TRUE;
            F_ack(ctx, BL_REQUEST_STATUS_COMPLETE, BL_ERROR_NONE);
            return BL_STATE_RESET;

        case BL_REQUEST_MUX_UPDATE_ABORT:
            F_clusterClear(ctx);
            ctx->wordsWritten = 0uL;
            ctx->clstrCrcRetryCount = 0u;
            APP_IF_imageRefresh(ctx);
            F_ack(ctx, BL_REQUEST_STATUS_COMPLETE, BL_ERROR_NONE);
            return BL_STATE_MENU;
        default:
            F_ack(ctx, BL_REQUEST_STATUS_INVALID, BL_ERROR_STATE);
            return state;
    }
}
