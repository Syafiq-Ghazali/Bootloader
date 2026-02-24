#include "UART.h"

void UART_init
(
    void
)
{

    Interrupt_register(INT_SCIA_RX, RX_interrupt);
    Interrupt_register(INT_SCIA_TX, TX_interrupt);
    Interrupt_enable(INT_SCIA_RX);
    Interrupt_enable(INT_SCIA_TX);


    SCI_setConfig(SCIA_BASE, DEVICE_LSPCLK_FREQ, SCI_BAUD_RATE, (SCI_CONFIG_WLEN_8 |
                                                        SCI_CONFIG_STOP_ONE |
                                                        SCI_CONFIG_PAR_NONE));
    SCI_enableModule(SCIA_BASE);
    SCI_resetChannels(SCIA_BASE);
    SCI_enableFIFO(SCIA_BASE);

    SCI_enableInterrupt(SCIA_BASE, (SCI_INT_RXFF | SCI_INT_TXFF));
    SCI_disableInterrupt(SCIA_BASE, SCI_INT_RXERR);

    SCI_setFIFOInterruptLevel(SCIA_BASE, SCI_FIFO_TX2, SCI_FIFO_RX2);
    SCI_performSoftwareReset(SCIA_BASE);

    SCI_resetTxFIFO(SCIA_BASE);
    SCI_resetRxFIFO(SCIA_BASE);

}

void __interrupt RX_interrupt
(
    void
)
{
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP9);
}

void __interrupt TX_interrupt
(
    void
)
{
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP9);
}
