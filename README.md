# CAN Bootloader

A CAN bootloader for the TI TMS320F280039C microcontroller, developed using Code Composer Studio.

It receives firmware updates over CAN, programs the application into flash, and uses CRC-32 checks to verify the firmware before running it.

This project is a work in progress. More documentation will be added as development continues.

See the [CAN command interface](ASW/APP_IF/PROTOCOL.md) for commands and the Python message format.

The [Python PCAN client](host/README.md) provides command-line tools for flashing, reading, erasing, and verifying applications.
