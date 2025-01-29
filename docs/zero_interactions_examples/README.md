# Blixt Zero Manual Request Tool

## Description
The software contained in the essential package are simple tools to interact with the Blixt Zero Switching Gear.

## Essentials

### Common
Here are all the common libraries and files needed the examples inside this directory.

### 01 - Basic GET Call
This example shows how to make a simple CoAP call to request information from the Zero SG device.
It sends an unencoded CoAP GET request to the targeted Zero, asking for the available CoAP resources.

### 02 - Protobuf Encoded GET Call
This example shows how to make a *Protobuf Encoded* CoAP call to request information from the Zero device.
It sends an encoded GET request to the targeted Zero, asking for the status of the device.

### 03 - Protobuf POST Call
This example shows how to make a *Profobuf Encoded* CoAP call to control the Zero device.
It sends an encoded POST request to the targeted Zero, requiring it to toggle the coupling state. 

### 04 - Protobuf POST Call - ERROR
This example shows how **NOT** to make a Profobuf Encoded CoAP call to control the Zero device.
It sends a **unencoded** POST request to the targeted Zero, and receives an error back. 

### 05 - Multiple interactions and calls demo
This example creates a menu, offering many call possibilities that you can select do execute to the CoAP device.

Take some time to look through the code and understand how it works, basically it is made for all examples
shown on the previous files, but with a few more tricks and tips.

Notice that you have to configure the device IP prior to use.

## Advanced

### 06 - Zero Auto Discovery
On the auto discovery example code, it is possible to see how to implement an automatic device discovery protocol 
using UDP broadcast.
The script will try to find all available devices for some time, after that it will list all the devices found.

This is derived from the "zero_cli_tool" code, in a more simpler and compact version.

End of File