from google.protobuf.internal import containers as _containers
from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from typing import ClassVar as _ClassVar, Iterable as _Iterable, Mapping as _Mapping, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class ZCApiVersion(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = []
    ZC_API_VERSION_1: _ClassVar[ZCApiVersion]
    ZC_API_VERSION_2: _ClassVar[ZCApiVersion]

class ZCSwitchState(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = []
    ZC_SWITCH_STATE_OPENED: _ClassVar[ZCSwitchState]
    ZC_SWITCH_STATE_CLOSED: _ClassVar[ZCSwitchState]

class ZCDeviceState(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = []
    ZC_DEVICE_STATE_UNDEFINED: _ClassVar[ZCDeviceState]
    ZC_DEVICE_STATE_OPENED: _ClassVar[ZCDeviceState]
    ZC_DEVICE_STATE_CLOSED: _ClassVar[ZCDeviceState]
    ZC_DEVICE_STATE_STANDBY: _ClassVar[ZCDeviceState]
    ZC_DEVICE_STATE_TRANSIENT: _ClassVar[ZCDeviceState]

class ZCTripCause(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = []
    ZC_TRIP_CAUSE_NONE: _ClassVar[ZCTripCause]
    ZC_TRIP_CAUSE_EXT: _ClassVar[ZCTripCause]
    ZC_TRIP_CAUSE_OCP_HW: _ClassVar[ZCTripCause]
    ZC_TRIP_CAUSE_OCP_CURVE: _ClassVar[ZCTripCause]
    ZC_TRIP_CAUSE_OCP_HW_TEST: _ClassVar[ZCTripCause]
    ZC_TRIP_CAUSE_OTP: _ClassVar[ZCTripCause]
    ZC_TRIP_CAUSE_UVP: _ClassVar[ZCTripCause]
    ZC_TRIP_CAUSE_OVP: _ClassVar[ZCTripCause]
    ZC_TRIP_CAUSE_UFP: _ClassVar[ZCTripCause]
    ZC_TRIP_CAUSE_OFP: _ClassVar[ZCTripCause]

class ZCFlowDirection(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = []
    ZC_FLOW_DIRECTION_FORWARD: _ClassVar[ZCFlowDirection]
    ZC_FLOW_DIRECTION_BACKWARD: _ClassVar[ZCFlowDirection]

class ZCDeviceCmd(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = []
    ZC_DEVICE_CMD_OPEN: _ClassVar[ZCDeviceCmd]
    ZC_DEVICE_CMD_CLOSE: _ClassVar[ZCDeviceCmd]
    ZC_DEVICE_CMD_TOGGLE: _ClassVar[ZCDeviceCmd]

class ZCTempLoc(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = []
    ZC_TEMP_LOC_AMB: _ClassVar[ZCTempLoc]
    ZC_TEMP_LOC_MCU_1: _ClassVar[ZCTempLoc]
    ZC_TEMP_LOC_MCU_2: _ClassVar[ZCTempLoc]
    ZC_TEMP_LOC_MCU_3: _ClassVar[ZCTempLoc]
    ZC_TEMP_LOC_MCU_4: _ClassVar[ZCTempLoc]
    ZC_TEMP_LOC_BRD_1: _ClassVar[ZCTempLoc]
    ZC_TEMP_LOC_BRD_2: _ClassVar[ZCTempLoc]
    ZC_TEMP_LOC_BRD_3: _ClassVar[ZCTempLoc]
    ZC_TEMP_LOC_BRD_4: _ClassVar[ZCTempLoc]

class ZCCalibType(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = []
    ZC_CALIB_TYPE_VOLTAGE_1: _ClassVar[ZCCalibType]
    ZC_CALIB_TYPE_VOLTAGE_2: _ClassVar[ZCCalibType]
    ZC_CALIB_TYPE_CURRENT_1: _ClassVar[ZCCalibType]
    ZC_CALIB_TYPE_CURRENT_2: _ClassVar[ZCCalibType]
ZC_API_VERSION_1: ZCApiVersion
ZC_API_VERSION_2: ZCApiVersion
ZC_SWITCH_STATE_OPENED: ZCSwitchState
ZC_SWITCH_STATE_CLOSED: ZCSwitchState
ZC_DEVICE_STATE_UNDEFINED: ZCDeviceState
ZC_DEVICE_STATE_OPENED: ZCDeviceState
ZC_DEVICE_STATE_CLOSED: ZCDeviceState
ZC_DEVICE_STATE_STANDBY: ZCDeviceState
ZC_DEVICE_STATE_TRANSIENT: ZCDeviceState
ZC_TRIP_CAUSE_NONE: ZCTripCause
ZC_TRIP_CAUSE_EXT: ZCTripCause
ZC_TRIP_CAUSE_OCP_HW: ZCTripCause
ZC_TRIP_CAUSE_OCP_CURVE: ZCTripCause
ZC_TRIP_CAUSE_OCP_HW_TEST: ZCTripCause
ZC_TRIP_CAUSE_OTP: ZCTripCause
ZC_TRIP_CAUSE_UVP: ZCTripCause
ZC_TRIP_CAUSE_OVP: ZCTripCause
ZC_TRIP_CAUSE_UFP: ZCTripCause
ZC_TRIP_CAUSE_OFP: ZCTripCause
ZC_FLOW_DIRECTION_FORWARD: ZCFlowDirection
ZC_FLOW_DIRECTION_BACKWARD: ZCFlowDirection
ZC_DEVICE_CMD_OPEN: ZCDeviceCmd
ZC_DEVICE_CMD_CLOSE: ZCDeviceCmd
ZC_DEVICE_CMD_TOGGLE: ZCDeviceCmd
ZC_TEMP_LOC_AMB: ZCTempLoc
ZC_TEMP_LOC_MCU_1: ZCTempLoc
ZC_TEMP_LOC_MCU_2: ZCTempLoc
ZC_TEMP_LOC_MCU_3: ZCTempLoc
ZC_TEMP_LOC_MCU_4: ZCTempLoc
ZC_TEMP_LOC_BRD_1: ZCTempLoc
ZC_TEMP_LOC_BRD_2: ZCTempLoc
ZC_TEMP_LOC_BRD_3: ZCTempLoc
ZC_TEMP_LOC_BRD_4: ZCTempLoc
ZC_CALIB_TYPE_VOLTAGE_1: ZCCalibType
ZC_CALIB_TYPE_VOLTAGE_2: ZCCalibType
ZC_CALIB_TYPE_CURRENT_1: ZCCalibType
ZC_CALIB_TYPE_CURRENT_2: ZCCalibType

class ZCVersion(_message.Message):
    __slots__ = ["api", "uuid", "sw_ver", "hw_ver", "link_addr", "sec_en"]
    API_FIELD_NUMBER: _ClassVar[int]
    UUID_FIELD_NUMBER: _ClassVar[int]
    SW_VER_FIELD_NUMBER: _ClassVar[int]
    HW_VER_FIELD_NUMBER: _ClassVar[int]
    LINK_ADDR_FIELD_NUMBER: _ClassVar[int]
    SEC_EN_FIELD_NUMBER: _ClassVar[int]
    api: ZCApiVersion
    uuid: bytes
    sw_ver: bytes
    hw_ver: bytes
    link_addr: bytes
    sec_en: bool
    def __init__(self, api: _Optional[_Union[ZCApiVersion, str]] = ..., uuid: _Optional[bytes] = ..., sw_ver: _Optional[bytes] = ..., hw_ver: _Optional[bytes] = ..., link_addr: _Optional[bytes] = ..., sec_en: bool = ...) -> None: ...

class ZCTemperature(_message.Message):
    __slots__ = ["loc", "value"]
    LOC_FIELD_NUMBER: _ClassVar[int]
    VALUE_FIELD_NUMBER: _ClassVar[int]
    loc: ZCTempLoc
    value: int
    def __init__(self, loc: _Optional[_Union[ZCTempLoc, str]] = ..., value: _Optional[int] = ...) -> None: ...

class ZCStatus(_message.Message):
    __slots__ = ["uptime", "switch_state", "device_state", "cause", "current", "voltage", "freq", "direction", "temp"]
    UPTIME_FIELD_NUMBER: _ClassVar[int]
    SWITCH_STATE_FIELD_NUMBER: _ClassVar[int]
    DEVICE_STATE_FIELD_NUMBER: _ClassVar[int]
    CAUSE_FIELD_NUMBER: _ClassVar[int]
    CURRENT_FIELD_NUMBER: _ClassVar[int]
    VOLTAGE_FIELD_NUMBER: _ClassVar[int]
    FREQ_FIELD_NUMBER: _ClassVar[int]
    DIRECTION_FIELD_NUMBER: _ClassVar[int]
    TEMP_FIELD_NUMBER: _ClassVar[int]
    uptime: int
    switch_state: ZCSwitchState
    device_state: ZCDeviceState
    cause: ZCTripCause
    current: int
    voltage: int
    freq: int
    direction: ZCFlowDirection
    temp: _containers.RepeatedCompositeFieldContainer[ZCTemperature]
    def __init__(self, uptime: _Optional[int] = ..., switch_state: _Optional[_Union[ZCSwitchState, str]] = ..., device_state: _Optional[_Union[ZCDeviceState, str]] = ..., cause: _Optional[_Union[ZCTripCause, str]] = ..., current: _Optional[int] = ..., voltage: _Optional[int] = ..., freq: _Optional[int] = ..., direction: _Optional[_Union[ZCFlowDirection, str]] = ..., temp: _Optional[_Iterable[_Union[ZCTemperature, _Mapping]]] = ...) -> None: ...

class ZCCurvePoint(_message.Message):
    __slots__ = ["limit", "duration"]
    LIMIT_FIELD_NUMBER: _ClassVar[int]
    DURATION_FIELD_NUMBER: _ClassVar[int]
    limit: int
    duration: int
    def __init__(self, limit: _Optional[int] = ..., duration: _Optional[int] = ...) -> None: ...

class ZCCsomModConfig(_message.Message):
    __slots__ = ["closed", "period"]
    CLOSED_FIELD_NUMBER: _ClassVar[int]
    PERIOD_FIELD_NUMBER: _ClassVar[int]
    closed: int
    period: int
    def __init__(self, closed: _Optional[int] = ..., period: _Optional[int] = ...) -> None: ...

class ZCCsomConfig(_message.Message):
    __slots__ = ["enabled", "mod"]
    ENABLED_FIELD_NUMBER: _ClassVar[int]
    MOD_FIELD_NUMBER: _ClassVar[int]
    enabled: bool
    mod: ZCCsomModConfig
    def __init__(self, enabled: bool = ..., mod: _Optional[_Union[ZCCsomModConfig, _Mapping]] = ...) -> None: ...

class ZCCurveConfig(_message.Message):
    __slots__ = ["points", "direction"]
    POINTS_FIELD_NUMBER: _ClassVar[int]
    DIRECTION_FIELD_NUMBER: _ClassVar[int]
    points: _containers.RepeatedCompositeFieldContainer[ZCCurvePoint]
    direction: ZCFlowDirection
    def __init__(self, points: _Optional[_Iterable[_Union[ZCCurvePoint, _Mapping]]] = ..., direction: _Optional[_Union[ZCFlowDirection, str]] = ...) -> None: ...

class ZCOcpHwConfig(_message.Message):
    __slots__ = ["limit", "filter", "rec_delay", "rec_attempts", "rec_en"]
    LIMIT_FIELD_NUMBER: _ClassVar[int]
    FILTER_FIELD_NUMBER: _ClassVar[int]
    REC_DELAY_FIELD_NUMBER: _ClassVar[int]
    REC_ATTEMPTS_FIELD_NUMBER: _ClassVar[int]
    REC_EN_FIELD_NUMBER: _ClassVar[int]
    limit: int
    filter: int
    rec_delay: int
    rec_attempts: int
    rec_en: bool
    def __init__(self, limit: _Optional[int] = ..., filter: _Optional[int] = ..., rec_delay: _Optional[int] = ..., rec_attempts: _Optional[int] = ..., rec_en: bool = ...) -> None: ...

class ZCOuvpConfig(_message.Message):
    __slots__ = ["lower", "upper", "enabled"]
    LOWER_FIELD_NUMBER: _ClassVar[int]
    UPPER_FIELD_NUMBER: _ClassVar[int]
    ENABLED_FIELD_NUMBER: _ClassVar[int]
    lower: int
    upper: int
    enabled: bool
    def __init__(self, lower: _Optional[int] = ..., upper: _Optional[int] = ..., enabled: bool = ...) -> None: ...

class ZCOufpConfig(_message.Message):
    __slots__ = ["lower", "upper", "enabled"]
    LOWER_FIELD_NUMBER: _ClassVar[int]
    UPPER_FIELD_NUMBER: _ClassVar[int]
    ENABLED_FIELD_NUMBER: _ClassVar[int]
    lower: int
    upper: int
    enabled: bool
    def __init__(self, lower: _Optional[int] = ..., upper: _Optional[int] = ..., enabled: bool = ...) -> None: ...

class ZCNotifConfig(_message.Message):
    __slots__ = ["interval"]
    INTERVAL_FIELD_NUMBER: _ClassVar[int]
    interval: int
    def __init__(self, interval: _Optional[int] = ...) -> None: ...

class ZCCalibConfig(_message.Message):
    __slots__ = ["type", "arg"]
    TYPE_FIELD_NUMBER: _ClassVar[int]
    ARG_FIELD_NUMBER: _ClassVar[int]
    type: ZCCalibType
    arg: bytes
    def __init__(self, type: _Optional[_Union[ZCCalibType, str]] = ..., arg: _Optional[bytes] = ...) -> None: ...

class ZCConfig(_message.Message):
    __slots__ = ["curve", "csom", "ocp_hw", "ouvp", "oufp", "notif", "calib"]
    CURVE_FIELD_NUMBER: _ClassVar[int]
    CSOM_FIELD_NUMBER: _ClassVar[int]
    OCP_HW_FIELD_NUMBER: _ClassVar[int]
    OUVP_FIELD_NUMBER: _ClassVar[int]
    OUFP_FIELD_NUMBER: _ClassVar[int]
    NOTIF_FIELD_NUMBER: _ClassVar[int]
    CALIB_FIELD_NUMBER: _ClassVar[int]
    curve: ZCCurveConfig
    csom: ZCCsomConfig
    ocp_hw: ZCOcpHwConfig
    ouvp: ZCOuvpConfig
    oufp: ZCOufpConfig
    notif: ZCNotifConfig
    calib: ZCCalibConfig
    def __init__(self, curve: _Optional[_Union[ZCCurveConfig, _Mapping]] = ..., csom: _Optional[_Union[ZCCsomConfig, _Mapping]] = ..., ocp_hw: _Optional[_Union[ZCOcpHwConfig, _Mapping]] = ..., ouvp: _Optional[_Union[ZCOuvpConfig, _Mapping]] = ..., oufp: _Optional[_Union[ZCOufpConfig, _Mapping]] = ..., notif: _Optional[_Union[ZCNotifConfig, _Mapping]] = ..., calib: _Optional[_Union[ZCCalibConfig, _Mapping]] = ...) -> None: ...

class ZCRequestVersion(_message.Message):
    __slots__ = ["null"]
    NULL_FIELD_NUMBER: _ClassVar[int]
    null: int
    def __init__(self, null: _Optional[int] = ...) -> None: ...

class ZCRequestStatus(_message.Message):
    __slots__ = ["null"]
    NULL_FIELD_NUMBER: _ClassVar[int]
    null: int
    def __init__(self, null: _Optional[int] = ...) -> None: ...

class ZCRequestDeviceCmd(_message.Message):
    __slots__ = ["cmd"]
    CMD_FIELD_NUMBER: _ClassVar[int]
    cmd: ZCDeviceCmd
    def __init__(self, cmd: _Optional[_Union[ZCDeviceCmd, str]] = ...) -> None: ...

class ZCRequestSetConfig(_message.Message):
    __slots__ = ["config"]
    CONFIG_FIELD_NUMBER: _ClassVar[int]
    config: ZCConfig
    def __init__(self, config: _Optional[_Union[ZCConfig, _Mapping]] = ...) -> None: ...

class ZCRequestGetConfigCurve(_message.Message):
    __slots__ = ["direction"]
    DIRECTION_FIELD_NUMBER: _ClassVar[int]
    direction: ZCFlowDirection
    def __init__(self, direction: _Optional[_Union[ZCFlowDirection, str]] = ...) -> None: ...

class ZCRequestGetConfigCsom(_message.Message):
    __slots__ = ["null"]
    NULL_FIELD_NUMBER: _ClassVar[int]
    null: int
    def __init__(self, null: _Optional[int] = ...) -> None: ...

class ZCRequestGetConfigOcpHw(_message.Message):
    __slots__ = ["null"]
    NULL_FIELD_NUMBER: _ClassVar[int]
    null: int
    def __init__(self, null: _Optional[int] = ...) -> None: ...

class ZCRequestGetConfigOuvp(_message.Message):
    __slots__ = ["null"]
    NULL_FIELD_NUMBER: _ClassVar[int]
    null: int
    def __init__(self, null: _Optional[int] = ...) -> None: ...

class ZCRequestGetConfigOufp(_message.Message):
    __slots__ = ["null"]
    NULL_FIELD_NUMBER: _ClassVar[int]
    null: int
    def __init__(self, null: _Optional[int] = ...) -> None: ...

class ZCRequestGetConfigNotif(_message.Message):
    __slots__ = ["null"]
    NULL_FIELD_NUMBER: _ClassVar[int]
    null: int
    def __init__(self, null: _Optional[int] = ...) -> None: ...

class ZCRequestGetConfigCalib(_message.Message):
    __slots__ = ["type"]
    TYPE_FIELD_NUMBER: _ClassVar[int]
    type: ZCCalibType
    def __init__(self, type: _Optional[_Union[ZCCalibType, str]] = ...) -> None: ...

class ZCRequestGetConfig(_message.Message):
    __slots__ = ["curve", "csom", "ocp_hw", "ouvp", "oufp", "notif", "calib"]
    CURVE_FIELD_NUMBER: _ClassVar[int]
    CSOM_FIELD_NUMBER: _ClassVar[int]
    OCP_HW_FIELD_NUMBER: _ClassVar[int]
    OUVP_FIELD_NUMBER: _ClassVar[int]
    OUFP_FIELD_NUMBER: _ClassVar[int]
    NOTIF_FIELD_NUMBER: _ClassVar[int]
    CALIB_FIELD_NUMBER: _ClassVar[int]
    curve: ZCRequestGetConfigCurve
    csom: ZCRequestGetConfigCsom
    ocp_hw: ZCRequestGetConfigOcpHw
    ouvp: ZCRequestGetConfigOuvp
    oufp: ZCRequestGetConfigOufp
    notif: ZCRequestGetConfigNotif
    calib: ZCRequestGetConfigCalib
    def __init__(self, curve: _Optional[_Union[ZCRequestGetConfigCurve, _Mapping]] = ..., csom: _Optional[_Union[ZCRequestGetConfigCsom, _Mapping]] = ..., ocp_hw: _Optional[_Union[ZCRequestGetConfigOcpHw, _Mapping]] = ..., ouvp: _Optional[_Union[ZCRequestGetConfigOuvp, _Mapping]] = ..., oufp: _Optional[_Union[ZCRequestGetConfigOufp, _Mapping]] = ..., notif: _Optional[_Union[ZCRequestGetConfigNotif, _Mapping]] = ..., calib: _Optional[_Union[ZCRequestGetConfigCalib, _Mapping]] = ...) -> None: ...

class ZCError(_message.Message):
    __slots__ = ["code"]
    CODE_FIELD_NUMBER: _ClassVar[int]
    code: int
    def __init__(self, code: _Optional[int] = ...) -> None: ...

class ZCResponse(_message.Message):
    __slots__ = ["version", "status", "config", "error"]
    VERSION_FIELD_NUMBER: _ClassVar[int]
    STATUS_FIELD_NUMBER: _ClassVar[int]
    CONFIG_FIELD_NUMBER: _ClassVar[int]
    ERROR_FIELD_NUMBER: _ClassVar[int]
    version: ZCVersion
    status: ZCStatus
    config: ZCConfig
    error: ZCError
    def __init__(self, version: _Optional[_Union[ZCVersion, _Mapping]] = ..., status: _Optional[_Union[ZCStatus, _Mapping]] = ..., config: _Optional[_Union[ZCConfig, _Mapping]] = ..., error: _Optional[_Union[ZCError, _Mapping]] = ...) -> None: ...

class ZCRequest(_message.Message):
    __slots__ = ["version", "status", "cmd", "set_config", "get_config"]
    VERSION_FIELD_NUMBER: _ClassVar[int]
    STATUS_FIELD_NUMBER: _ClassVar[int]
    CMD_FIELD_NUMBER: _ClassVar[int]
    SET_CONFIG_FIELD_NUMBER: _ClassVar[int]
    GET_CONFIG_FIELD_NUMBER: _ClassVar[int]
    version: ZCRequestVersion
    status: ZCRequestStatus
    cmd: ZCRequestDeviceCmd
    set_config: ZCRequestSetConfig
    get_config: ZCRequestGetConfig
    def __init__(self, version: _Optional[_Union[ZCRequestVersion, _Mapping]] = ..., status: _Optional[_Union[ZCRequestStatus, _Mapping]] = ..., cmd: _Optional[_Union[ZCRequestDeviceCmd, _Mapping]] = ..., set_config: _Optional[_Union[ZCRequestSetConfig, _Mapping]] = ..., get_config: _Optional[_Union[ZCRequestGetConfig, _Mapping]] = ...) -> None: ...

class ZCMessage(_message.Message):
    __slots__ = ["req", "res"]
    REQ_FIELD_NUMBER: _ClassVar[int]
    RES_FIELD_NUMBER: _ClassVar[int]
    req: ZCRequest
    res: ZCResponse
    def __init__(self, req: _Optional[_Union[ZCRequest, _Mapping]] = ..., res: _Optional[_Union[ZCResponse, _Mapping]] = ...) -> None: ...
