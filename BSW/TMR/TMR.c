/*
 * Millisecond timer service for the bootloader.
 */

#include <stdint.h>
#include "TMR.h"

static Uint32_t F_freeRunningTmr = 0u;

void TMR_initialize
(
    void
)
{
    CPUTimer_setEmulationMode(CPUTIMER0_BASE, CPUTIMER_EMULATIONMODE_STOPAFTERNEXTDECREMENT);
    CPUTimer_setPreScaler(CPUTIMER0_BASE, 0u);
    CPUTimer_setPeriod(CPUTIMER0_BASE, TMR0_PERIOD);
    CPUTimer_stopTimer(CPUTIMER0_BASE);
    CPUTimer_reloadTimerCounter(CPUTIMER0_BASE);

    Interrupt_register((Uint32_t)INT_TIMER0, TMR_tmr0Isr);
    CPUTimer_enableInterrupt(CPUTIMER0_BASE);
    Interrupt_enable((Uint32_t)INT_TIMER0);

    CPUTimer_startTimer(CPUTIMER0_BASE);
}

Uint32_t TMR_freeRunningTmrRead
(
    void
)
{
    return F_freeRunningTmr;
}

RAMFUNC void TMR_freqMeasure
(
    const Uint32_t prevTime,
    const Uint32_t currTime,
    Float32_t *freqOut
)
{
    Float32_t timeDiff = 0.0f;

    if (freqOut == NULL)
    {
        return;
    }

    if (currTime >= prevTime)
    {
        timeDiff = (Float32_t)(currTime - prevTime);
    }
    else
    {
        timeDiff = (Float32_t)((UINT32_MAX - prevTime) + currTime + 1uL);
    }

    if ((Uint32_t)timeDiff != 0u)
    {
        *freqOut = 1.0f / (timeDiff * TMR_FREE_RUNNING_TMR0_TICK_TIME);
    }
    else
    {
        *freqOut = 0.0f;
    }
}

void TMR_createAndStart
(
    TmrHandle_t *tmr,
    Uint32_t durationMs
)
{
    if (tmr != NULL)
    {
        tmr->durationMs = durationMs;
        tmr->timerExpired = FALSE;
        tmr->startTime = F_freeRunningTmr;
    }
}

void TMR_update
(
    TmrHandle_t *tmr
)
{
    Uint32_t timeDiff = 0u;
    Uint32_t currTime = F_freeRunningTmr;

    if (tmr == NULL)
    {
        return;
    }

    if (currTime >= tmr->startTime)
    {
        timeDiff = currTime - tmr->startTime;
    }
    else
    {
        timeDiff = (UINT32_MAX - tmr->startTime) + currTime + 1uL;
    }

    tmr->timerExpired = (timeDiff >= tmr->durationMs);
}

Bool_t TMR_checkExpiry
(
    TmrHandle_t *tmr
)
{
    return (tmr != NULL) ? tmr->timerExpired : TRUE;
}

Bool_t TMR_updateAndCheckExpiry
(
    TmrHandle_t *tmr
)
{
    TMR_update(tmr);
    return TMR_checkExpiry(tmr);
}

__interrupt void TMR_tmr0Isr
(
    void
)
{
    F_freeRunningTmr++;
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP1);
}
