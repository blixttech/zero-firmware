# Blixt Zero SG
## Device Characteristics

### Introduction

The Blixt Zero Switching Gear responds to interactions for data enquiries and device control and configuration via CoAP protocol. This document will list ways to interact with the device, which resources can be called among other details.

Depending on the resource called you may get an enconded message that needs to be decoded in a specific way, also described on this document.

### CoAP
 CoAP is an application layer protocol that works in a similar way to RESTful, working with request and response fashion, support services such as  discovery of resources and others, if you still don't know much about it, there are a few sources listed bellow that can be used as introduction.

- [RFC 7252 - CoAP specification](https://www.ietf.org/rfc/rfc7252.txt)
- [CoAP Documentation and Tutorials](https://coap.space/)
- [CoAP on Wikipedia](https://en.wikipedia.org/wiki/Constrained_Application_Protocol)

### Protocol Buffer (Profobuf)
On top of the CoAP protocol the actual metrics and control are carried as ProtoBuf encoded messages, so even if you make CoAP requests to the correct resource, you might get an unintelligible response or no response at all if not encoded correctly.

In order to have those messages encoded or decoded, it is needed to generate the library for the language that you're using to make the requests. The library can be generated from ProtoFiles (included in this repository) for many languages, some are supported by Google, some others supported by the community.

If you wish to learn morea about ProtoBuf and generating libraries, you may find a few links bellow. **Note that the documentation refers to Python language libraries**, but there are other supported languages and has documents available on other parts of those web pages.

- [Protobuf Overview](https://protobuf.dev/overview/)

### ProtoBuf - Compiling a Library
Have in mind that when compiling the library, if it's meant to be used with Python, 
it is important to generate python stubs (.pyi) as well. For that the use of the flag `pyi_out` is necessary.
- [Compiling Library - Python](https://protobuf.dev/getting-started/pythontutorial/#compiling-protocol-buffers)

The Protobuf library can be generated from the following protofiles:
- [Zero Control Messages](https://github.com/blixttech/zero-control-messages/tree/main)

    The important files to do this are "zc_messages.proto" and "zc_messages.options".

#### Protobuf - Document on Payload Translation
- [Protobuf Python Library Reference](https://googleapis.dev/python/protobuf/latest/google/protobuf/message.html#google.protobuf.message.Message.MergeFromString)

### Network Address

The device acquires its IP address via DHCP, although it should respond to Network discovery requests sent
via UDP protocol throughout the network.

## CoAP API Available Resources

Blixt Zero SG CoAP API offers the resources.  

| Resource | Supported request type |
| :--- | :--- |
| [`.well-known/core`](#available_resources) | GET |
| [`version`](#version) | GET |
| [`status`](#status) | GET, observable |
| [`config`](#config) | GET, POST |
| [`device`](#device) | GET, POST |

Except for the ".well-known/core" endpoint that has the response body in CSV (**C**omma-**S**eparated **V**alues), all the other endpoints will respond a with Protobuf encoded message, that has to be decoded in order to be read. Also the endpoints that accept POST requests needs to receive a Protobuf encoded request.

## Required Headers and Options

Also note the reply when ``.well-know/core`` endpoint is requested:

``</version>;ct=30001,</status>;obs;ct=30001,</config>;ct=30001,</device>;ct=30001``


#### Content Format: 30001

You must set the Content-Format option in the CoAP request to 30001, The Content-Format option in CoAP (Constrained Application Protocol) 
is used to specify the media type and content encoding of the request or response body. 

#### From the RFC 7252, section 5.10.3. Content-Format
"The Content-Format Option indicates the representation format of the message payload.  The representation format is given as a numeric
Content-Format identifier that is defined in the "CoAP Content-Formats" registry (Section 12.3).  In the absence of the option, no
default value is assumed, i.e., the representation format of any representation message payload is indeterminate."

### Observable Option

On CoAP it is possible to register a client (consumer) to receive updates from the server (device).
For the Blixt Zero SG Device, the parameters are:

| Parameter | Value |
| :--- | :--- |
Maximum Consumers |  4
Protocol Type | UDP
Time to Auto De-Register | 1 Minute

### Device Network Discovery

To query for Blixt Devices on a network, the process is quite straightforward.

Sending CoAP GET requests to the  `.well-known/core` endpoint with the destination to the broadcast address
of the connected network should cause the devices to respond to the source address of the request.

The  `.well-known/core` call is described bellow.

### Verbs Accepted by the Endpoints

The verbs accepted by the Blixt Zero Switching Gear are:
- GET 
- POST
  
There are no other verbs implemented to be used in the device's firmware.

## API Endpoints, Requests and Responses

### `.well-known/core` - GET

This endpoint responds a list of every availabe endpoint to be called.

Request:

    Verb: GET 
    
    Endpoint: coap://<zero-sg-ip-address>/.well-known/core

    Data Format: String of Bytes
    
    Payload Content: None

    Content-Format: None

Response:

    Format: Unencoded Text
    Value: </version>;ct=30001,</status>;obs;ct=30001,</config>;ct=30001,</device>;ct=30001

**Note:** it also informs the Content-Type/Content-Format to be used on each as needed, described above.


### `version` - GET 

**Observations:**
- This endpoint forms the reply content using Protobuf, that shall be decoded.

#### Request:

    Verb: GET 
    
    Endpoint: coap://<zero-sg-ip-address>/version
    
    Payload Response Example:
    
        NONE

    Content-Format: 30001

#### Response:

    Format: Protobuf Encoded Text

    Value:

        version {
            uuid: "\000\000\000\000\000\000\000\000\123EN\000\000\000p"
            link_addr: "\000\000\000\000\000\000"
        }


### `status` - GET, Observable

**Observations:**
- This endpoint has to be called using a ProtoBuf Payload.
- This endpoint can be used to the client to register as an obsever. 

#### Request:

    Verb: VERB
    
    Endpoint: coap://<zero-sg-ip-address>/example
    
    Payload Response Example:

        NONE

    Content-Format: 30001
#### Response:

    Format: Protobuf Encoded Data
    
    Value: 

        status {
            uptime: 3226188
            switch_state: ZC_SWITCH_STATE_CLOSED
            device_state: ZC_DEVICE_STATE_CLOSED
            cause: ZC_TRIP_CAUSE_EXT
            current: 21
            voltage: 233900
            freq: 50022
            temp {
                value: 30
            }
            temp {
                loc: ZC_TEMP_LOC_MCU_1
                value: 34
            }
            temp {
                loc: ZC_TEMP_LOC_BRD_1
                value: 30
            }
            temp {
                loc: ZC_TEMP_LOC_BRD_2
                value: 31
            }
        }


### `config` - GET, POST

**Observations:**
- This endpoint forms the reply content using Protobuf, that shall be decoded.
- This endpoint has to be called using a ProtoBuf encoded payload.
- The of the Content-Format option in the CoAP must be set, with the value as 30001.

#### Request:

    Verb: VERB
    
    Endpoint: coap://<zero-sg-ip-address>/example
    
    Payload Content: None

    Content-Format: 30001
#### Response:

    Format: Protobuf Encoded Data
    Value: 

### `device` - POST

**Observations:**
- This endpoint forms the reply content using Protobuf, that shall be decoded.
- This endpoint has to be called using a ProtoBuf encoded payload.
- The of the Content-Format option in the CoAP must be set, with the value as 30001.

#### Request:

    Verb: VERB
    
    Endpoint: coap://<zero-sg-ip-address>/example
    
    Payload Content: None

    Content-Format: 30001
#### Response:

    Format: Protobuf Encoded Data
    Value: 

API Specification Documentation
Request and Response Details
Tripping Time and Over+voltage in API Status
Package Call Frequency



### `example` - VERB

#### Request:

    Verb: VERB
    
    Endpoint: coap://<zero-sg-ip-address>/example
    
    Payload Content: None

    Content-Format: 30001

#### Response:

    Format: Protobuf Encoded Data
    Value: 