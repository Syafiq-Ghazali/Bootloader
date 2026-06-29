/*
 * Millisecond timer service for the bootloader.
 */

#ifndef _TMR_H_
#define _TMR_H_

#include "BootloaderDefs.h"
#include "device.h"

#define TMR0_FREQ                       (1000u)
#define TMR0_PERIOD                     ((Uint32_t)(DEVICE_SYSCLK_FREQ / ((Uint32_t)TMR0_FREQ)))
#define TMR_FREE_RUNNING_TMR0_TICK_TIME ((Float32_t)(1.0f / TMR0_FREQ))

typedef struct TmrHandle_t
{
    Uint32_t startTime;
    Uint32_t durationMs;
    Bool_t timerExpired;
} TmrHandle_t;

extern void        TMR_initialize            (void);
extern Uint32_t    TMR_freeRunningTmrRead    (void);
extern void        TMR_freqMeasure           (const Uint32_t prevTime,
                                              const Uint32_t currTime,
                                              Float32_t *freqOut);
extern void        TMR_createAndStart        (TmrHandle_t *tmr,
                                              Uint32_t durationMs);
extern void        TMR_update                (TmrHandle_t *tmr);
extern Bool_t      TMR_checkExpiry           (TmrHandle_t *tmr);
extern Bool_t      TMR_updateAndCheckExpiry  (TmrHandle_t *tmr);
extern __interrupt void TMR_tmr0Isr          (void);

#endif
