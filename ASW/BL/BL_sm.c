/* Boot lifecycle and CAN request scheduling. Commands are handled by APP_IF. */
#include <string.h>
#include "BL_sm.h"
#include "APP_IF.h"
#include "TMR.h"

static CtxData_t F_ctx;
static SmHandle_t F_sm =
{
    .self = (void *)&F_sm,
    .currState = BL_STATE_STARTUP,
    .prevState = BL_STATE_STARTUP,
    .nextState = BL_STATE_STARTUP
};
static TmrHandle_t F_timeout;
static Bool_t F_timerActive;

static void F_report(void)
{
    BL_ifReplySysStatusTx(F_ctx.vMetadata.appCrc, F_ctx.appValid, F_ctx.errorReason);
}

Uint16_t BL_smInitialize(void)
{
    memset(&F_ctx, 0, sizeof(F_ctx));
    F_sm.self = (void *)&F_sm;
    F_sm.currState = BL_STATE_STARTUP;
    F_sm.prevState = BL_STATE_STARTUP;
    F_sm.nextState = BL_STATE_STARTUP;
    F_timerActive = FALSE;
    return OK;
}

SmState_t BL_smStateRetreive(void)
{
    return F_sm.currState;
}

Uint16_t BL_smStart(void)
{
    Uint16_t result;
    do { result = BL_smExe(); } while (result == OK);
    return result;
}

Uint16_t BL_smExe(void)
{
    if (F_sm.self != (void *)&F_sm) return FAIL;
    F_sm.nextState = F_sm.currState;
    switch (F_sm.currState)
    {
        case BL_STATE_STARTUP:
            BL_imgFixedMetadataRead(&F_ctx.fMetadata);
            APP_IF_imageRefresh(&F_ctx);
            F_ctx.errorReason = F_ctx.appValid ? BL_ERROR_NONE : BL_ERROR_APP_CRC;
            BL_ifReplyVersionTx(F_ctx.fMetadata.bootloaderVersion, F_ctx.vMetadata.appVersion,
                                F_ctx.fMetadata.hardwareVersion);
            F_report();
            F_sm.nextState = BL_STATE_MENU;
            break;

        case BL_STATE_MENU:
        case BL_STATE_UPDATE:
            if (!F_timerActive)
            {
                TMR_createAndStart(&F_timeout, F_sm.currState == BL_STATE_MENU ?
                                   BL_MENU_TIMEOUT_MS : BL_TARGET_UPDATE_TIMEOUT_MS);
                F_timerActive = TRUE;
            }
            // Poll once so other application work can share the main loop later.
            if ((BL_ifRx(&F_ctx.currMsg) == OK) &&
                (F_ctx.currMsg.type != BL_IF_MESSAGE_REPLY))
            {
                SmState_t next = APP_IF_process(&F_ctx, F_sm.currState);
                F_timerActive = FALSE;
                F_sm.nextState = next;
            }
            else if (TMR_updateAndCheckExpiry(&F_timeout))
            {
                F_timerActive = FALSE;
                if (F_sm.currState == BL_STATE_UPDATE)
                {
                    BL_clstrInit();
                    F_ctx.clstrReady = FALSE;
                    F_ctx.clstrCommitted = FALSE;
                    F_ctx.clstrWordsLoaded = 0u;
                    APP_IF_imageRefresh(&F_ctx);
                    F_ctx.errorReason = BL_ERROR_TARGET_UPDATE_TIMEOUT;
                    F_report();
                    F_sm.nextState = BL_STATE_MENU;
                }
                else if (F_ctx.appValid) F_sm.nextState = BL_STATE_APP_RUN;
                // An invalid/erased image stays in the menu, waiting for CAN.
            }
            break;

        case BL_STATE_RESET:
        case BL_STATE_APP_RUN:
            if (!F_timerActive)
            {
                TMR_createAndStart(&F_timeout, BL_CAN_TX_PENDING_TIMEOUT_MS);
                F_timerActive = TRUE;
            }
            if (BL_ifTxPending() && !TMR_updateAndCheckExpiry(&F_timeout)) break;
            if (F_sm.currState == BL_STATE_RESET) SysCtl_resetDevice();
            else if (F_ctx.appValid)
            {
                DISABLE_GLOBAL_INTERRUPT();
                BL_imgAppStart();
                // The application entry point must not return.
                F_sm.nextState = BL_STATE_ERROR;
            }
            else F_sm.nextState = BL_STATE_MENU;
            break;

        default:
        case BL_STATE_ERROR:
            BL_clstrInit();
            F_ctx.clstrReady = FALSE;
            F_ctx.clstrCommitted = FALSE;
            F_ctx.clstrWordsLoaded = 0u;
            APP_IF_imageRefresh(&F_ctx);
            F_report();
            F_sm.nextState = BL_STATE_MENU;
            break;
    }
    if (F_sm.nextState != F_sm.currState)
    {
        F_sm.prevState = F_sm.currState;
        F_sm.currState = F_sm.nextState;
        F_timerActive = FALSE;
    }
    return OK;
}
