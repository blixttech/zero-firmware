# Infineon Serial Inspection and Configuration Interface (SICI)

## Introduction

Serial Inspection and Configuration Interface (SICI) is used for configuring TLx4971 and TLE4972
family current sensors.
SICI is functionally similar to the 1-wire communication bus.
However, SICI devices use the same IO pins for both digital communication and other analog
signals.
Therefore, pins used for digital communication and analog signals require isolation from each other
on the MCU.

## UART based implementation

Some MCUs have native 1-wire peripherals which could be used to implement SICI. 
However, this driver uses the conventional UART driver to implement SICI due to the
increased portability.

In this implementation, TX and RX pins of the UART are connected together such that 
received bits depend on the response from the connected device.
If there is no device connected, sent and received bits become same.
To this end, the TX pin of the UART needs to be in open-drain output mode
with pull-up enabled.
If the UART does not support this mode, an external buffer with open-drain output and pull-up
may be used.
If an external output buffer is used, additional analog switch is required to isolate digital and analog IO pins.

The following table summarises the pin configuration settings for different modes when the MCU pins are directly used with the connected device.

| **Mode** | **UART TX** | **UART RX** |
|---|---|---|
| Default | Analogue input or disabled | Analogue input or disabled |
| Interface activation | Digital output (open-drain), pull-up enabled | Digital input |
| Communication | UART TX (open-drain), pull-up enabled | UART RX |

The following table summarises the pin configuration settings for different modes when the MCU pins are used via an external output buffer with the connected device.

| **Mode** | **UART TX** | **UART RX** |
|---|---|---|
| Default | N/A (disconnected via switch) | N/A (disconnected via switch) |
| Interface activation | Digital output | Digital input |
| Communication | UART TX | UART RX |


As in the protocol specification, the connected device sends back a bit for each bit receives. 

Baudrate and the bits sent by the UART determine the timing for interface enabling sequence 
and sending/receiving bits.
To this end, this driver uses 1 start bit, 8 data bits,
no parity bit and 1 stop bit (10 bits in total) configuration.
In this way, sending and receiving a data bit over SICI requires the UART to 
send 2 data bytes (20 bits in total).

Additionally, a GPIO pin is used to do the power-on reset prior to sending 
the interface activation sequence. 

## Timing

The following parameters are extracted from the programming guide of TLI4971.

| **Parameter** | **Symbol** | **Min** |  **Typical** | **Max** | **Unit** 
|---|---|---|---|---|---|
| Interface enable time | $t_{ifen}$ | 100 | 150 | 400 | $us$ 
| Period time of a bit | $T$ | 40 | $t_{1\_x}+t_{2\_x}$ | 7500 | us
| Low time - bit 0 | $t_{1\_0}$ | 28 | 33 | 38 | % of $T$ 
| High time - bit 0 | $t_{2\_0}$ | 62 | 67 | 72 | % of $T$
| Low time - bit 1 | $t_{1\_1}$ | 62 | 67 | 72 | % of $T$
| High time - bit 1 | $t_{2\_1}$ | 28 | 33 | 38 | % of $T$
| Low time before read | $t_3$ | 10 | - | 30 | % of $t_4$
| Read time | $t_r$ | 50 | - | 80 | % of $t_4$
| Response time | $t_4$ | $2*abs(t_{1\_x}-t_{2\_x})$ | - | - | $us$
| Time between 2 bits | $t_5$ | 1 | $T-t_4$ | 5400 | $us$
| Max high time | $t_{high}$ | - | - | 5400 | us
| Max low time | $t_{low}$ | - | - | 5400 | us