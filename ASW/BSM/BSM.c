#include "BSM.h"

static void F_stateTransition(SmHandle_t *handle);

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
void BSM_fsm(SmHandle_t *handle)
{
    switch (handle->currState)
    {
        case BL_STATE_INIT:
            break;

        case BL_STATE_START:
        // WILL NOT RETURN
            BAP_jump();
            break;

        case BL_TIMER_START

            break;
        
        
        default:
            break;
    }

    F_stateTransition(handle);
}
