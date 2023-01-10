# CoAP API

CoAP API consists of following resources.  

| Resource | Supported request type |
| :--- | :--- |
| [`version`](#version) | GET |
| [`status`](#status) | GET, observable |
| [`switch`](#swtich) | GET, POST |
| [`tc`](#tc) | GET, POST |
| [`tc_def`](#tc_def) | GET, POST |


Responses to all requests are in Comma-separated values (CSV) format.

Following describes how to test the API using `coap-client` command-line interface (CLI) client.

* Install `coap-client`. For *Ubuntu*-based systems use the following command to install.
  ```bash
  sudo apt install libcoap2-bin
  ```
* The CLI client can be used as follows.
  ```bash
  coap-client -m <method> "coap://<device IP>/<resource>?<query parameters>"

  ```
  * *&lt;method&gt;* &ndash; CoAP method. Valid values: `get` or `post`.
  * *&lt;device IP&gt;* &ndash; IP address of the device.
  * *&lt;resource&gt;* &ndash; Refer to the above table.
  * *&lt;query parameters&gt;* &ndash; This is a tuple of name-value pairs (separated by `&`) in the format *&lt;name1&gt;=&lt;value1&gt;&&lt;name2&gt;=&lt;value2&gt;...*.
    * *&lt;name1&gt;, &lt;name2&gt;, ...* &ndash; Refer to the parameters of the individual resource.
    * *&lt;value1&gt;, &lt;value2&gt;, ...* &ndash; Refer to the supported values of the related parameters.
    * If only one name-value pair is specified, use the format *&lt;name&gt;=&lt;value&gt;*.

## `version`
Get hardware IDs.

### GET parameters
None.

### Response
| Index | Description |
| :--- | :--- |
| 0 | MCU UUID. |
| 1 | Ethernet MAC address. |
| 2 | Power-in board revision. |
| 3 | Power-out board revision. |
| 4 | Controller board revision. |


## `status`
Get switch status, trip curve status and measurements.

### GET parameters (only if the observe option is set)
| Parameter | Description |
| :--- | :--- |
| p | Set the observation notification interval in milliseconds. Value range: 100 &ndash; 2<sup>32</sup>-1.  |

### Response
| Index | Description |
| :--- | :--- |
| 0 | Uptime in milliseconds. |
| 1 | Switch status. Values: <ul><li>0 &ndash; Opened</li><li> 1 &ndash; Closed</li></ul> |
| 2 | Cause for the trip curve to change its status. Values: <ul><li>0 &ndash; None</li><li>1 &ndash; External user event</li><li>2 &ndash; Hardware-based overcurrent protection</li><li>3 &ndash; Software-based overcurrent protection</li><li>4 &ndash; Hardware-based overcurrent protection test</li><li>5 &ndash; Over temperature protection</li><li>6 &ndash; Under voltage protection</li></ul> |
| 3 | Trip curve state. Values: <ul><li>0 &ndash; Undefined</li><li> 1 &ndash; Opened</li><li> 2 &ndash; Closed</li><li> 3 &ndash; Transient</li></ul> |
| 4 | Instantaneous current (mA). |
| 5 | RMS current (mA). |
| 6 | Instantaneous voltage (mV). |
| 7 | RMS voltage (mA). |
| 8 | Power-in board temperature (&#8451;). |
| 9 | Power-out board temperature (&#8451;). |
| 10 | Ambient temperature inside the enclosure (&#8451;). |
| 11 | MCU temperature (&#8451;). |


## `switch`
Get status and perform open/close operations.

### GET parameters
None.

### POST parameters
| Parameter | Description |
| :--- | :--- |
| `a` | Set trip curve state. Values: `open`, `close`, `toggle` |

### Response
Refer to the [response](#response-1) format of the [status](#status) resource.


## `tc`
Get status of current trip curve, set its parameters and perform open/close operations.

### GET parameters
None.

### POST parameters
| Parameter | Description |
| :--- | :--- |
| `a` | Set trip curve state. Values: `open`, `close`, `toggle` |
| `hwl` | Set hardware current limit. Value range: 30 &ndash; 60 |

### Response
| Index | Description |
| :--- | :--- |
| 0 | Trip curve state. Refer to the [response](#response-1) format of the [status](#status) resource. |
| 1 | Cause for the trip curve to change its status. Refer to the [response](#response-1) format of the [status](#status) resource. |
| 2 | Hardware current limit (A). |

## `tc_def`
Get status of default trip curve and set its parameters.

### GET parameters
None.

### POST parameters
| Parameter | Description |
| :--- | :--- |
| `recen` | Enable/disable recovery. Values: <ul><li>0 &ndash; Disable</li><li> 1 &ndash; Enable</li></ul> |
| `rec` | Set the number of recovery attempts. Values: 0 &ndash; 65535 |
| `csom` | Set the close-state operation mode. Values:  <ul><li>0 &ndash; Disabled</li><li> 1 &ndash; Sine wave modulation control</li></ul> |
| `csomcl` | Applicable only for sine wave modulation control mode. </br>Set the number of voltage zero-crossings that the switch stays closed. Values: 1 &ndash; 254  |
| `csomper` | Applicable only for sine wave modulation control mode. </br>Set the number of voltage zero-crossings to be considered as a period. Values: 2 &ndash; 255  |

### Response
| Index | Description |
| :--- | :--- |
| 0 &ndash; 2 | Refer to the [response](#response-3) format of [tc](#tc) resource. |
| 3 | Indicates if the recovery is enabled. Refer to the POST parameter description of `recen` field. |
| 4 | Number of recovery attempts. Refer to the POST parameter description of `rec` field. |
| 5 | Close-state operation mode. Refer to the POST parameter description of `csom` field. |
| 6 | Number of voltage zero-crossings that the switch stays closed. Refer to the POST parameter description of `csomcl` field. |
| 7 | Number of voltage zero-crossings to be considered as a period. Refer to the POST parameter description of `csomper` field. |

## Developer note
This API is subjected to change to group the functionalities among CoAP resources in a more meaningful way.