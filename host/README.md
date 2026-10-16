# Python PCAN Client

A Python tool for updating firmware over CAN using a PEAK PCAN adapter.
It supports flashing, reading, erasing, CRC checks, and device control.

Defaults: `PCAN_USBBUS1` at **500 kbit/s**.

## Setup

Use Python 3.10 or newer with the PEAK driver and
[PCAN-Basic](https://www.peak-system.com/products/software/development-packages/pcan-basic/?L=1) installed.

Run these commands from the **Bootloader** folder in PowerShell:

```powershell
py -m venv host/.venv
.\host\.venv\Scripts\python.exe -m pip install -e ./host
```

## Usage

Reset the board and send a command within its 8-second bootloader window.

```powershell
.\host\.venv\Scripts\canboot.exe info
.\host\.venv\Scripts\canboot.exe status
.\host\.venv\Scripts\canboot.exe flash application.bin --version 1
.\host\.venv\Scripts\canboot.exe --help
```

Flashing erases the application, writes the new firmware, checks its CRC, and resets the board.
Use a raw little-endian `.bin` application linked to start at word address `0x85000`.

More details: [CAN command interface](../ASW/APP_IF/PROTOCOL.md).
