#include "BSM.h"

static void F_stateTransition(SmHandle_t *handle);
static void F_fsm(SmHandle_t *handle);


/* State Transition */
static void F_stateTransition(SmHandle_t *handle)
{
    if (handle->currState != handle->nextState)
    {
        handle->prevState = handle->currState;
        handle->currState = handle->nextState;
    }
}


/* Main FSM */
static void F_fsm(SmHandle_t *handle)
{
    switch (handle->currState)
    {
        case BL_STATE_INIT:
            BL_init();
            handle->nextState = BL_STATE_READ_METADATA;
            break;

        case BL_STATE_READ_METADATA:
            BL_metadataRead(&handle->metadata);
            handle->nextState = BL_STATE_DECIDE_BOOT;
            break;

        case BL_STATE_DECIDE_BOOT:
            if (BL_pendingImageExists(&handle->metadata))
            {
                if (BL_pendingImageValid(&handle->metadata) &&
                    BL_bootAttemptsRemaining(&handle->metadata))
                {
                    BL_incrementBootAttempts(&handle->metadata);
                    BL_metadataWrite(&handle->metadata);
                    handle->nextState = BL_STATE_LAUNCH_APP;
                }
                else
                {
                    handle->nextState = BL_STATE_ROLLBACK;
                }
            }
            else if (BL_activeImageValid(&handle->metadata))
            {
                handle->targetSlot = BL_getActiveSlot(&handle->metadata);
                handle->nextState = BL_STATE_WAIT_FOR_UPDATE;
            }
            else
            {
                handle->nextState = BL_STATE_WAIT_FOR_UPDATE;
            }
            break;

        case BL_STATE_WAIT_FOR_UPDATE:
            if (BL_updateRequestReceived())
            {
                handle->nextState = BL_STATE_RECEIVE_IMAGE;
            }
            else if (BL_activeImageValid(&handle->metadata))
            {
                handle->targetSlot = BL_getActiveSlot(&handle->metadata);
                handle->nextState = BL_STATE_LAUNCH_APP;
            }
            else
            {
                handle->nextState = BL_STATE_WAIT_FOR_UPDATE;
            }
            break;

        case BL_STATE_RECEIVE_IMAGE:
            if (BL_receiveImage(handle))
            {
                handle->nextState = BL_STATE_VALIDATE_IMAGE;
            }
            else
            {
                handle->nextState = BL_STATE_ERROR;
            }
            break;

        case BL_STATE_VALIDATE_IMAGE:
            if (BL_validateReceivedImage(handle))
            {
                handle->nextState = BL_STATE_MARK_PENDING;
            }
            else
            {
                handle->nextState = BL_STATE_ERROR;
            }
            break;

        case BL_STATE_MARK_PENDING:
            BL_markImagePending(&handle->metadata, handle->inactiveSlot);
            BL_metadataWrite(&handle->metadata);
            handle->nextState = BL_STATE_SWAP_IMAGES;
            break;

        case BL_STATE_SWAP_IMAGES:
            BL_setActiveSlot(&handle->metadata, handle->inactiveSlot);
            BL_metadataWrite(&handle->metadata);
            handle->targetSlot = handle->inactiveSlot;
            handle->nextState = BL_STATE_LAUNCH_APP;
            break;

        case BL_STATE_LAUNCH_APP:
            BL_jumpToApp(handle->targetSlot);
            break;

        case BL_STATE_ROLLBACK:
            BL_restorePreviousImage(&handle->metadata);
            BL_metadataWrite(&handle->metadata);
            handle->targetSlot = BL_getActiveSlot(&handle->metadata);
            handle->nextState = BL_STATE_LAUNCH_APP;
            break;

        case BL_STATE_ERROR:
        default:
            BL_handleError();
            break;
    }

    F_stateTransition(handle);
}