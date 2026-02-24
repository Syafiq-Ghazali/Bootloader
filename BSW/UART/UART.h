#ifndef _MY_UART_
#define _MY_UART_

/* Includes */
#include "sci.h"
#include "device.h"
#include "board.h"
#include "driverlib.h"


/* Defines */

#define SCI_BAUD_RATE           (9600)
#define SCI_DATA_LENGTH         (5)
#define SCI_DATA                ()


/* Extern Functions */
extern void UART_init(void);
extern __interrupt void RX_interrupt(void);
extern __interrupt void TX_interrupt(void);


#endif
