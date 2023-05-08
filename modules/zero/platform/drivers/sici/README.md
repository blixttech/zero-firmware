# Infineon Serial Inspection and Configuration Interface (SICI)

## Introduction

Serial Inspection and Configuration Interface (SICI) is used for configuring TLx4971 and TLE4972
family current sensors.
SICI is functionally similar to the 1-wire communication bus.
However, SICI devices use the same IO pins for both digital communication and other analog
signals.
Therefore, pin used for digital communication and analog signals require isolation from each other
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
If the UART does not support this mode, an external buffer with open-drain output and pull up
may be used.
If an external buffer is used, additional analog switch may be required for isolate isolate
digital and analog IO pins.

As in the protocol specification, the connected device sends back a bit for each bit receives. 

Baudrate and the bits sent by the UART determine the timing for interface enabling sequence 
and sending/receiving bits.
To this end, this driver uses 1 start bit, 8 data bits,
no parity bit and 1 stop bit (10 bits in total) configuration.
In this way, sending and receiving a data bit over SICI requires the UART to 
send 2 data bytes (20 bits in total).

Additionally, a GPIO pin is used to do the power-on reset prior to sending 
the interface activation sequence. 