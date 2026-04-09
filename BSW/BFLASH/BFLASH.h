/* Include Files */

#include "driverlib.h"
#include "device.h"
#include "F021_F28003x_C28x.h"

/* Flash Wait states */
#define BFLASH_WAITSTATES (5u)

/* Extern Function */
extern void BFLASH_init(void);
extern void BFLASH_clear(void);
extern void BFLASH_write(void);
