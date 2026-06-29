/*
 * Shared bootloader definitions.
 */

#ifndef _BOOTLOADERDEFS_H_
#define _BOOTLOADERDEFS_H_

#include "Datatype.h"

#define DISABLE_GLOBAL_INTERRUPT()                  {DINT;}
#define ENABLE_GLOBAL_INTERRUPT()                   {EINT;}
#define DISABLE_GLOBAL_REALTIME_INTERRUPT_DBGM()    {DRTM;}
#define ENABLE_GLOBAL_REALTIME_INTERRUPT_DBGM()     {ERTM;}

#define RAMFUNC __attribute__((ramfunc))

#ifndef OK
#define OK      ((Uint16_t)0u)
#endif

#ifndef SUCCESS
#define SUCCESS ((Uint16_t)0u)
#endif

#ifndef FAIL
#define FAIL    ((Uint16_t)0xFFFFu)
#endif

#ifndef FAILURE
#define FAILURE ((Uint16_t)0xFFFFu)
#endif

#ifndef NULL
#define NULL    ((void *)0u)
#endif

#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#define IS_EQUAL(a, b)      ((a) == (b))
#define SET_BIT(val, mask)  ((val) |=  (mask))
#define CLR_BIT(val, mask)  ((val) &= ~(mask))
#define TEST_BIT(val, mask) (IS_EQUAL(((val) & (mask)), (mask)))

#endif
