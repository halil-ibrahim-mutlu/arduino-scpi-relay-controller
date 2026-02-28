# Arduino SCPI Relay Controller

Professional SCPI-based multi-channel relay controller for Arduino.

This project implements standard SCPI commands to control digital outputs remotely over a Serial interface.

---

## ✨ Features

- Multi-channel digital output control
- Standard SCPI command structure
- Numeric suffix channel addressing (OUTP0, OUTP1...)
- Output state query support
- *IDN?, *RST, *CLS, SYST:ERR? commands
- ALL channel control

---

## 🖥 Hardware

- Arduino Uno / Nano
- Relay module or digital output driver
- Serial connection (9600 baud)

Channel mapping:

| Channel | Arduino Pin |
|----------|------------|
| CH0 | D2 |
| CH1 | D3 |
| CH2 | D4 |
| CH3 | D5 |
| CH4 | D6 |
| CH5 | D7 |
| CH6 | D8 |

---

## 📡 SCPI Command Examples
