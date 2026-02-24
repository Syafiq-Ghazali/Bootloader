
/*Include Files */
#include "main.h"
#include "BLS.h"
#include "UART.h"
#include "BUSB.h"
#include "BSPI.h"
#include "BLS.h"

/* Defines */


/* Main Loop */
void main(void)
{

    Device_init();
    Device_initGPIO();
    Interrupt_initModule();
    Interrupt_initVectorTable();
    Board_init();
    C2000Ware_libraries_init();


    /* Enable UART */
    UART_init();

    /* Enable Interupt */ 
    EINT;
    ERTM;



    BLS_init();
    
    /* Should never reach this state */
    while(1)
    {
        
    }
}

//
// End of File
//
