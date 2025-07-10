# EtherNet/IP Protocol Reference

## Overview

This document provides a comprehensive reference for the EtherNet/IP (Ethernet Industrial Protocol) and CIP (Common Industrial Protocol) implementation across different PLC manufacturers and types. It covers protocol structures, packet flows, and vendor-specific implementations including Rockwell Automation (ControlLogix, CompactLogix, MicroLogix, PLC5, SLC), OMRON (NJ/NX series), and legacy PCCC/DF1 protocols.

## Protocol Stack

```
┌─────────────────────────────────────┐
│           Application Layer         │
│    (Tag Read/Write, PCCC, etc.)     │
├─────────────────────────────────────┤
│             CIP Layer              │
│  (Services, Objects, Attributes)    │
├─────────────────────────────────────┤
│     Common Packet Format (CPF)     │
│    (Connected/Unconnected Data)     │
├─────────────────────────────────────┤
│        EtherNet/IP Layer           │
│      (Encapsulation Header)         │
├─────────────────────────────────────┤
│             TCP Layer              │
│          (Port 44818)              │
├─────────────────────────────────────┤
│           Ethernet/IP              │
└─────────────────────────────────────┘
```

## EtherNet/IP Encapsulation Layer

### Encapsulation Header

```c
/**
 * EtherNet/IP Encapsulation Header
 * All fields are little-endian unless specified
 */
typedef struct __attribute__((packed)) {
    uint16_t command;           // EIP command (ListIdentity=0x0063, RegisterSession=0x0065, etc.)
    uint16_t length;            // Length of data following this header
    uint32_t session_handle;    // Session identifier (0 for unregistered commands)
    uint32_t status;            // Status code (0 = success)
    uint64_t sender_context;    // Echo data for request/response matching
    uint32_t options;           // Options flags (typically 0)
} eip_encap_header_t;

/**
 * EIP Command Codes
 */
#define EIP_CMD_NOP                 0x0000
#define EIP_CMD_LIST_SERVICES       0x0001
#define EIP_CMD_LIST_IDENTITY       0x0063
#define EIP_CMD_LIST_INTERFACES     0x0064
#define EIP_CMD_REGISTER_SESSION    0x0065
#define EIP_CMD_UNREGISTER_SESSION  0x0066
#define EIP_CMD_SEND_RR_DATA        0x006F  // Unconnected messaging
#define EIP_CMD_SEND_UNIT_DATA      0x0070  // Connected messaging
```

### Session Management

#### Register Session

```c
/**
 * Register Session Request/Response
 */
typedef struct __attribute__((packed)) {
    uint16_t protocol_version;  // Always 1
    uint16_t options;          // Typically 0
} eip_register_session_t;
```

**Session Setup Process:**
1. Client sends `RegisterSession` command
2. Server responds with unique session handle
3. All subsequent communications use this session handle
4. Session timeout varies by manufacturer (typically 30-60 seconds)

#### Device Discovery (ListIdentity)

**ListIdentity Request (Command 0x0063):**
```c
/**
 * List Identity Request - Broadcast UDP packet
 */
typedef struct __attribute__((packed)) {
    uint16_t command;           // 0x0063 (ListIdentity)
    uint16_t length;            // 0x0000 (no data)
    uint32_t session_handle;    // 0x00000000 (no session)
    uint32_t status;            // 0x00000000
    uint64_t sender_context;    // Optional context for response matching
    uint32_t options;           // 0x00000000
} list_identity_request_t;

/**
 * List Identity Response Item
 */
typedef struct __attribute__((packed)) {
    uint16_t item_type;                    // 0x000C for Identity
    uint16_t item_length;                  // Length of following data
    uint16_t encapsulation_protocol_version; // Little-endian
    int16_t  sin_family;                   // Big-endian (AF_INET = 2)
    uint16_t sin_port;                     // Big-endian (44818)
    uint8_t  sin_addr[4];                  // IP address in network byte order (big-endian)
    uint8_t  sin_zero[8];                  // Padding
    uint16_t vendor_id;                    // Little-endian (1=Rockwell, 47=OMRON)
    uint16_t device_type;                  // Little-endian (14=PLC, 12=Comm Adapter)
    uint16_t product_code;                 // Little-endian
    uint8_t  revision_major;               // Major revision
    uint8_t  revision_minor;               // Minor revision
    uint16_t status;                       // Device status bitfield
    uint32_t serial_number;                // Device serial number
    uint8_t  product_name_length;          // Length of product name
    uint8_t  product_name[];               // Product name (variable length)
    uint8_t  state;                        // Device state
} list_identity_response_item_t;
```

**Device Discovery Protocol Flow:**
```
Client                                    Network Devices
  |                                             |
  |-- ListIdentity (UDP Broadcast) ----------->|
  |   Command: 0x0063                          |
  |   Port: 44818                              |
  |   IP: 255.255.255.255                      |
  |                                            |
  |<-- ListIdentity Response (UDP) ------------|
  |   Contains device identity information     |
  |   Multiple responses from different devices|
  |                                            |
```

**Discovery Implementation Details:**
- **Transport**: UDP broadcast on port 44818
- **Subnet Scanning**: Broadcast on all network interfaces
- **Response Timeout**: Typically 500ms - 1000ms
- **Multiple Responses**: Each device responds individually
- **Context Matching**: Use sender_context for response correlation

### CIP Vendor Identification

**Major CIP Vendors:**
```c
/**
 * CIP Vendor ID Registry (Partial List)
 */
typedef struct {
    uint16_t vendor_id;
    const char *vendor_name;
} cip_vendor_t;

static const cip_vendor_t CIP_VENDORS[] = {
    // Major PLC Vendors
    {1,   "Rockwell Automation/Allen-Bradley"},
    {47,  "OMRON Corporation"},
    {8,   "Molex Incorporated"},
    {26,  "Festo SE & Co KG"},
    {29,  "OPTO 22"},
    {40,  "WAGO Corporation"},
    {108, "Beckhoff Automation"},
    {252, "OMRON Software Co., Ltd."},
    {371, "OMRON ASO Co., Ltd"},
    {405, "OMRON Robotics and Safety Technologies, Inc."},
    {1095, "OMRON Microscan Systems, Inc."},

    // Industrial Automation Vendors
    {2,   "Namco Controls Corp."},
    {3,   "Honeywell International Inc."},
    {4,   "Parker Hannifin Corporation"},
    {5,   "Rockwell Automation/Reliance Elec."},
    {7,   "SMC Corporation"},
    {10,  "Advanced Micro Controls Inc. (AMCI)"},
    {11,  "ASCO Pneumatic Controls"},
    {12,  "Banner Engineering Corporation"},
    {13,  "Belden Wire & Cable Company"},
    {14,  "Cooper Interconnect"},
    {16,  "Daniel Woodhead Co."},
    {17,  "Dearborn Group Inc."},
    {19,  "Helm Instrument Company"},
    {20,  "Huron Net Works"},
    {21,  "Belden Deutschland GmbH"},
    {22,  "Online Development, Inc. (OLDI)"},
    {23,  "Vorne Industries, Inc."},
    {24,  "ODVA"},
    {30,  "Unico, Inc."},
    {31,  "Ross Controls"},
    {34,  "Hohner Corp."},
    {35,  "Micro Mo Electronics, Inc."},
    {36,  "MKS Instruments, Inc."},
    {37,  "Yaskawa Electric America formerly Magnetek Drives"},
    {39,  "AVG Automation (Uticor)"},
    {48,  "TURCK"},
    {58,  "Spectrum Controls, Inc."},
    {68,  "Eaton Electrical"},
    {78,  "Escort Memory Systems (A Datalogic Group Co.)"},
    {128, "MAC Valves, Inc."},
    {138, "Conxall Corporation Inc."},
    {148, "Honda Engineering Co., Ltd"},
    {158, "KVASER AB"},
    {178, "Toyo Denki Seizo KK"},
    {188, "A&D Company Limited"},
    {198, "Advanced Energy Industries, Inc."},
    {208, "Moog Inc."},
    {218, "NIDEC SANKYO CORPORATION (Sankyo Seiki Mfg. Co., Ltd)"},
    {228, "QLOG Corporation"},
    {238, "Simplatroll Ltd"},
    {248, "Eaton Industries GmbH (formerly Moeller GmbH)"},
    {258, "Hardy Process Solutions"},
    {278, "Sherrex Systems Ltd"},
    {298, "Tokyo Electron Software Technologies Ltd."},
    {308, "Western Servo Design Inc."},
    {328, "Okaya Electronics Corporation"},
    {338, "Dart Container Corp."},
    {348, "Berger Lahr GmbH"},
    {358, "Surface Combustion, Inc."},
    {368, "Kuroda Precision Industries Ltd."},
    {378, "Toyooki Kogyo Co., Ltd."},
    {398, "Selectron Systems AG"},
    {408, "ABB Welding Systems AB"},
    {418, "LP-Elektronik GmbH"},
    {428, "Giddings & Lewis, Inc."},
    {438, "Erhardt+Leimer GmbH"},
    {448, "Axiomatic Technologies Corp"},
    {458, "Taiwan Keiso Co., Ltd"},
    {468, "Baumer IVO GmbH & Co. KG"},
    {478, "Shimadzu Corporation"},
    {488, "Riken Keiki Co., Ltd."},
    {498, "Shimaden Co. Ltd."},
    {508, "Norcott Technologies Ltd"},
    {518, "Michiproducts Co., Ltd."},
    {528, "Cyberlogic Technologies"},
    {538, "Tescom Corporation"},
    {548, "JMACS"},
    {558, "Southwest Research Institute"},
    {568, "Suetron Electronic GmbH"},
    {578, "Smartscan Ltd"},
    {588, "West Instruments Limited"},
    {598, "Yokoyama Shokai Co., Ltd."},
    {608, "Tandis Research, Inc."},
    {618, "Milwaukee Electronics"},
    {628, "JEONGIL INTERCOM CO., LTD"},
    {638, "Woodhead Connectivity"},
    {648, "BL Autotec, Ltd."},
    {658, "Aucos GmbH"},
    {668, "Rockwell Automation/Entek IRD Intl."},
    {678, "Cognex Corporation"},
    {698, "Matsushita Electric Works, Ltd."},
    {708, "Yosio Electronic Co."},
    {718, "Niigata Electronic Instruments Co., Ltd."},
    {728, "Yamato Scale Co., Ltd."},
    {738, "Osaka Vacuum, Ltd."},
    {748, "DVT Corporation"},
    {758, "Dainippon Screen Mfg. Co. Ltd."},
    {768, "CSIRO Mining Automation"},
    {778, "HARTING, Inc. of North America"},
    {788, "RyuSyo Industrial Co., Ltd."},
    {798, "TE Connectivity"},
    {808, "SICK AG"},
    {818, "Trend Controls Systems Ltd."},
    {828, "ONO SOKKI CO.,LTD."},
    {838, "Celesco Transducer Products, Inc."},
    {848, "Heraeus Noblelight Fusion UV Inc."},
    {858, "Commercial Timesharing Inc."},
    {878, "Mitomi Giken Co., Ltd."},
    {888, "Hiprom Technologies"},
    {908, "HSD SpA"},
    {918, "Kun Hung Electric Co. Ltd."},
    {928, "GE Multilin"},
    {948, "ControlNet International"},
    {958, "SymCom, Inc."},
    {968, "IBA AG"},
    {988, "NIIGATA POWER SYSTEMS Co., Ltd."},
    {998, "Emerson Network Power Co., Ltd."},
    {1008, "DDK Ltd."},
    {1018, "Komatsu Electronics Inc."},
    {1028, "KVC Co., Ltd."},
    {1038, "Hurletron Inc."},
    {1048, "ORMEC Systems Corp."},
    {1058, "Eshed Technology"},
    {1078, "Automated Packing Systems"},
    {1088, "Trinite Automatisering B.V."},
    {1098, "Tiefenbach Control Systems GmbH"},
    {1108, "Meggitt Airdynamics, Inc."},
    {1118, "Technology Brewing Corporation"},
    {1138, "ASCON S.p.A."},
    {1158, "ARC Informatique"},
    {1168, "Kunbus GmbH Industrial Communication"},
    {1178, "CXR Ltd."},
    {1188, "Control Concepts Inc."},
    {1198, "Anton Paar GmbH"},
    {1208, "OES, Inc."},
    {1218, "SMA Railway Technology GmbH"},
    {1228, "embeX GmbH"},
    {1238, "Global Engineering Solutions Co., Ltd."},
    {1268, "HORIBA Precision Instruments (Beijing) Co.,Ltd."},
    {1288, "Shibaura Machine.co.,LTD"},
    {1308, "duagon AG"},
    {1318, "ABB S.p.A. - SACE Division"},
    {1328, "Fisher-Rosemount Systems, Inc. doing business as Process Systems & Solutions"},
    {1338, "Codenomicon"},
    {1348, "AIMCO"},
    {1358, "Hauch & Bach ApS"},
    {1368, "Nanotec Electronic GmbH & Co. KG"},
    {1378, "SEIDENSHA ELECTRONICS CO., LTD"},
    {1398, "Beijing Tianma Intelligent Control Technology Co., Ltd"},
    {1408, "ELAP S.R.L."},
    {1418, "Emerson Automation Solutions"},
    {1428, "SERVO-ROBOT INC."},
    {1438, "FLSmidth A/S"},
    {1448, "Grace Technologies"},
    {1458, "Perle Systems Limited"},
    {1468, "Campbell Wrapper Corporation"},
    {1488, "Vision Automation A/S"},
    {1498, "ASA-RT s.r.l"},
    {1518, "NetTechnix E&P GmbH"},
    {1528, "INGENIA-CAT, SL"},
    {1538, "LINAK Denmark A/S"},
    {1548, "Solar Turbines Incorporated"},
    {1558, "swisca ag"},
    {1568, "Hammond Power Solutions Inc."},
    {1578, "SICK OPTEX"},
    {1598, "Inspekto A.M.V LTD"},
    {1608, "Cetek"},
    {1618, "Techman Robot"},
    {1628, "Dover Flexo Electronics"},
    {1638, "Hach Lange SARL"},
    {1648, "MIDAS TECHNOLOGY Co.,Ltd"},
    {1658, "Christ Electronic Systems GmbH"},
    {1668, "MUSCLE CORPORATION"},
    {1678, "AGI Suretrack"},
    {1688, "Neurala, Inc."},
    {1698, "Sanwa Engineering Corp."},
    {1708, "Digital Dynamics, Inc."},
    {1728, "NTN TECHNICAL SERVICE"},
    {1738, "Reverity Inc"},
    {1748, "FOCUS-ON VoF"},
    {1768, "UGL Engineering Pty Limited"},
    {1778, "NIRECO CORPORATION"},
    {1788, "TMSS France"},
    {1988, "Unitronics (1989) (RG) LTD"},
    {0,    NULL}  // End marker
};
```

### CIP Device Type Classification

**Standard CIP Device Types:**
```c
/**
 * CIP Device Type Registry
 */
typedef struct {
    uint16_t device_type;
    const char *device_name;
    const char *description;
} cip_device_type_t;

static const cip_device_type_t CIP_DEVICE_TYPES[] = {
    {0x00, "Generic Device", "Generic Device (deprecated)"},
    {0x02, "AC Drive", "AC Drive"},
    {0x03, "Motor Overload", "Motor Overload Protection"},
    {0x04, "Limit Switch", "Limit Switch"},
    {0x05, "Inductive Proximity Switch", "Inductive Proximity Switch"},
    {0x06, "Photoelectric Sensor", "Photoelectric Sensor"},
    {0x07, "General Purpose Discrete I/O", "General Purpose Discrete I/O"},
    {0x09, "Resolver", "Resolver"},
    {0x0C, "Communications Adapter", "Communications Adapter"},
    {0x0E, "Programmable Logic Controller", "Programmable Logic Controller"},
    {0x10, "Position Controller", "Position Controller"},
    {0x13, "DC Drive", "DC Drive"},
    {0x15, "Contactor", "Contactor"},
    {0x16, "Motor Starter", "Motor Starter"},
    {0x17, "Soft Start", "Soft Start"},
    {0x18, "Human-Machine Interface", "Human-Machine Interface"},
    {0x1A, "Mass Flow Controller", "Mass Flow Controller"},
    {0x1B, "Pneumatic Valve", "Pneumatic Valve"},
    {0x1C, "Vacuum Pressure Gauge", "Vacuum Pressure Gauge"},
    {0x1D, "Process Control Value", "Process Control Value"},
    {0x1E, "Residual Gas Analyzer", "Residual Gas Analyzer"},
    {0x1F, "DC Power Generator", "DC Power Generator"},
    {0x20, "RF Power Generator", "RF Power Generator"},
    {0x21, "Turbomolecular Vacuum Pump", "Turbomolecular Vacuum Pump"},
    {0x22, "Encoder", "Encoder"},
    {0x23, "Safety Discrete I/O Device", "Safety Discrete I/O Device"},
    {0x24, "Fluid Flow Controller", "Fluid Flow Controller"},
    {0x25, "CIP Motion Drive", "CIP Motion Drive"},
    {0x26, "CompoNet Repeater", "CompoNet Repeater"},
    {0x27, "Mass Flow Controller, Enhanced", "Mass Flow Controller, Enhanced"},
    {0x28, "CIP Modbus Device", "CIP Modbus Device"},
    {0x29, "CIP Modbus Translator", "CIP Modbus Translator"},
    {0x2A, "Safety Analog I/O Device", "Safety Analog I/O Device"},
    {0x2B, "Generic Device (keyable)", "Generic Device (keyable)"},
    {0x2C, "Managed Switch", "Managed Switch"},
    {0x32, "ControlNet Physical Layer Component", "ControlNet Physical Layer Component"},
    {0,    NULL, NULL}  // End marker
};
```

**Device Status Bitfield:**
```c
/**
 * Device Status Bits (from ListIdentity response)
 */
#define DEVICE_STATUS_OWNED                 0x0001  // Device is owned
#define DEVICE_STATUS_CONFIGURED            0x0004  // Device is configured
#define DEVICE_STATUS_EXTENDED_DEVICE_STATUS 0x0010  // Extended device status
#define DEVICE_STATUS_MINOR_RECOVERABLE_FAULT 0x0100  // Minor recoverable fault
#define DEVICE_STATUS_MINOR_UNRECOVERABLE_FAULT 0x0200  // Minor unrecoverable fault
#define DEVICE_STATUS_MAJOR_RECOVERABLE_FAULT 0x0400  // Major recoverable fault
#define DEVICE_STATUS_MAJOR_UNRECOVERABLE_FAULT 0x0800  // Major unrecoverable fault

/**
 * Device Status Interpretation
 */
typedef struct {
    uint16_t status_bits;
    bool is_owned;
    bool is_configured;
    bool has_extended_status;
    bool minor_recoverable_fault;
    bool minor_unrecoverable_fault;
    bool major_recoverable_fault;
    bool major_unrecoverable_fault;
} device_status_t;

static inline device_status_t decode_device_status(uint16_t status) {
    device_status_t decoded = {
        .status_bits = status,
        .is_owned = (status & DEVICE_STATUS_OWNED) != 0,
        .is_configured = (status & DEVICE_STATUS_CONFIGURED) != 0,
        .has_extended_status = (status & DEVICE_STATUS_EXTENDED_DEVICE_STATUS) != 0,
        .minor_recoverable_fault = (status & DEVICE_STATUS_MINOR_RECOVERABLE_FAULT) != 0,
        .minor_unrecoverable_fault = (status & DEVICE_STATUS_MINOR_UNRECOVERABLE_FAULT) != 0,
        .major_recoverable_fault = (status & DEVICE_STATUS_MAJOR_RECOVERABLE_FAULT) != 0,
        .major_unrecoverable_fault = (status & DEVICE_STATUS_MAJOR_UNRECOVERABLE_FAULT) != 0
    };
    return decoded;
}
```

**Device State Values:**
```c
/**
 * Device State (last byte of ListIdentity response)
 */
#define DEVICE_STATE_NONEXISTENT       0x00  // Device does not exist
#define DEVICE_STATE_DEVICE_SELF_TESTING 0x01  // Device self testing
#define DEVICE_STATE_STANDBY           0x02  // Standby
#define DEVICE_STATE_OPERATIONAL       0x03  // Operational
#define DEVICE_STATE_MAJOR_FAULT       0x04  // Major fault
#define DEVICE_STATE_CONFIGURATION     0x05  // Configuration/setup
#define DEVICE_STATE_WAITING_FOR_RESET 0x06  // Waiting for reset

static const char* device_state_names[] = {
    "Nonexistent",
    "Self Testing",
    "Standby",
    "Operational",
    "Major Fault",
    "Configuration",
    "Waiting for Reset"
};
```

### Wire-Level Device Discovery Example

**Broadcast Discovery Request:**
```
UDP Packet (Client -> 255.255.255.255:44818)
EIP Header (24 bytes):
  0x0000: 63 00 00 00 00 00 00 00  // Command=0x0063, Length=0x0000
  0x0008: 00 00 00 00 EF CD AB 90  // Session=0, Status=0, Context
  0x0010: 78 56 34 12 00 00 00 00  // Context cont., Options=0
```

**Device Response Example (ControlLogix):**
```
UDP Packet (Device -> Client)
EIP Header (24 bytes):
  0x0000: 63 00 36 00 00 00 00 00  // Command=0x0063, Length=0x0036
  0x0008: 00 00 00 00 EF CD AB 90  // Session=0, Status=0, Context
  0x0010: 78 56 34 12 00 00 00 00  // Context cont., Options=0

CPF Items (8 bytes):
  0x0018: 00 00 00 00 0C 00 2A 00  // Null item, Identity item (length=42)

Identity Data (42 bytes):
  0x0020: 01 00 02 00 2E AF C0 A8  // Version=1, Family=2, Port=44818
  0x0028: 01 64 00 00 00 00 00 00  // IP=192.168.1.100, Padding
  0x0030: 01 00 0E 00 96 00 10 10  // Vendor=1, DeviceType=14, Product=150
  0x0038: 00 00 00 00 12 34 56 78  // Revision=16.16, Status=0, Serial
  0x0040: 14 31 37 36 39 2D 4C 36  // NameLen=20, Name="1769-L6..."
  0x0048: 35 45 52 4D 2F 42 20 4C  //
  0x0050: 6F 67 69 78 35 35 36 37  //
  0x0058: 03                       // State=3 (Operational)
```

**Device Response Example (OMRON NJ):**
```
UDP Packet (Device -> Client)
EIP Header (24 bytes):
  0x0000: 63 00 32 00 00 00 00 00  // Command=0x0063, Length=0x0032
  0x0008: 00 00 00 00 EF CD AB 90  // Session=0, Status=0, Context
  0x0010: 78 56 34 12 00 00 00 00  // Context cont., Options=0

CPF Items (8 bytes):
  0x0018: 00 00 00 00 0C 00 26 00  // Null item, Identity item (length=38)

Identity Data (38 bytes):
  0x0020: 01 00 02 00 2E AF C0 A8  // Version=1, Family=2, Port=44818
  0x0028: 01 C8 00 00 00 00 00 00  // IP=192.168.1.200, Padding
  0x0030: 2F 00 0E 00 45 02 01 01  // Vendor=47, DeviceType=14, Product=581
  0x0038: 00 00 00 00 AB CD EF 12  // Revision=1.1, Status=0, Serial
  0x0040: 0C 4E 4A 35 30 31 2D 31  // NameLen=12, Name="NJ501-1..."
  0x0048: 34 30 30 03             // State=3 (Operational)
```

## Common Packet Format (CPF) Layer

### CPF Header Structure

```c
/**
 * Common Packet Format Header
 */
typedef struct __attribute__((packed)) {
    uint16_t item_count;        // Number of CPF items (usually 2)
    // CPF items follow
} cpf_header_t;

/**
 * CPF Item Header
 */
typedef struct __attribute__((packed)) {
    uint16_t item_type;         // Item type code
    uint16_t item_length;       // Length of item data
    // Item data follows
} cpf_item_header_t;

/**
 * CPF Item Type Codes
 */
#define CPF_ITEM_NULL                   0x0000  // Null address item
#define CPF_ITEM_LIST_IDENTITY          0x000C  // List Identity response
#define CPF_ITEM_CONNECTION_BASED       0x00A1  // Connection-based address
#define CPF_ITEM_CONNECTED_DATA         0x00B1  // Connected data item
#define CPF_ITEM_UNCONNECTED_DATA       0x00B2  // Unconnected data item
#define CPF_ITEM_LIST_SERVICES          0x0100  // List Services response
#define CPF_ITEM_SOCK_ADDR_O_TO_T       0x8000  // Socket address O->T
#define CPF_ITEM_SOCK_ADDR_T_TO_O       0x8001  // Socket address T->O
#define CPF_ITEM_SEQUENCED_ADDRESS      0x8002  // Sequenced address
```

### Unconnected Messaging

```c
/**
 * Unconnected Data Item Structure
 */
typedef struct __attribute__((packed)) {
    // CPF Items (2 items)
    uint16_t address_item_type;     // 0x0000 (NULL)
    uint16_t address_item_length;   // 0x0000
    uint16_t data_item_type;        // 0x00B2 (Unconnected Data)
    uint16_t data_item_length;      // Length of CIP data
    // CIP request/response data follows
} unconnected_cpf_t;
```

### Connected Messaging

```c
/**
 * Connected Data Item Structure
 */
typedef struct __attribute__((packed)) {
    // CPF Items (2 items)
    uint16_t address_item_type;     // 0x00A1 (Connection-based)
    uint16_t address_item_length;   // 0x0004
    uint32_t connection_id;         // Connection identifier
    uint16_t data_item_type;        // 0x00B1 (Connected Data)
    uint16_t data_item_length;      // Length of CIP data
    uint16_t sequence_number;       // Sequence number (first 2 bytes of data)
    // CIP request/response data follows
} connected_cpf_t;
```

## CIP (Common Industrial Protocol) Layer

### CIP Request Structure

```c
/**
 * CIP Request Header
 */
typedef struct __attribute__((packed)) {
    uint8_t  service;           // CIP service code
    uint8_t  path_size;         // Path size in 16-bit words
    uint8_t  path[];            // IOI path (variable length)
    // Service-specific data follows
} cip_request_header_t;

/**
 * CIP Response Header
 */
typedef struct __attribute__((packed)) {
    uint8_t  service;           // Service code with reply bit (0x80)
    uint8_t  reserved;          // Reserved (0x00)
    uint8_t  general_status;    // General status code
    uint8_t  additional_status_size; // Size of additional status
    uint16_t additional_status[]; // Additional status (variable length)
    // Response data follows
} cip_response_header_t;
```

### CIP Service Codes

```c
/**
 * Standard CIP Services
 */
#define CIP_SERVICE_GET_ATTRIBUTE_ALL       0x01
#define CIP_SERVICE_SET_ATTRIBUTE_ALL       0x02
#define CIP_SERVICE_GET_ATTRIBUTE_LIST      0x03
#define CIP_SERVICE_SET_ATTRIBUTE_LIST      0x04
#define CIP_SERVICE_RESET                   0x05
#define CIP_SERVICE_START                   0x06
#define CIP_SERVICE_STOP                    0x07
#define CIP_SERVICE_CREATE                  0x08
#define CIP_SERVICE_DELETE                  0x09
#define CIP_SERVICE_MULTIPLE_SERVICE_PACKET 0x0A
#define CIP_SERVICE_APPLY_ATTRIBUTES        0x0D
#define CIP_SERVICE_GET_ATTRIBUTE_SINGLE    0x0E
#define CIP_SERVICE_SET_ATTRIBUTE_SINGLE    0x10
#define CIP_SERVICE_FIND_NEXT_OBJECT        0x11
#define CIP_SERVICE_RESTORE                 0x15
#define CIP_SERVICE_SAVE                    0x16
#define CIP_SERVICE_NO_OPERATION            0x17
#define CIP_SERVICE_GET_MEMBER              0x18
#define CIP_SERVICE_SET_MEMBER              0x19
#define CIP_SERVICE_INSERT_MEMBER           0x1A
#define CIP_SERVICE_REMOVE_MEMBER           0x1B
#define CIP_SERVICE_GROUP_SYNC              0x1C

/**
 * Tag Services (Rockwell Extensions)
 */
#define CIP_SERVICE_READ_TAG                0x4C
#define CIP_SERVICE_WRITE_TAG               0x4D
#define CIP_SERVICE_READ_TAG_FRAGMENTED     0x52
#define CIP_SERVICE_WRITE_TAG_FRAGMENTED    0x53
#define CIP_SERVICE_READ_MODIFY_WRITE_TAG   0x4E

/**
 * Advanced Tag Services
 */
#define CIP_SERVICE_GET_INSTANCE_ATTRIBUTE_LIST 0x55
#define CIP_SERVICE_GET_INSTANCE_LIST_EXTENDED  0x5F
```

### CIP Status Codes

```c
/**
 * CIP General Status Codes
 */
#define CIP_STATUS_SUCCESS                  0x00
#define CIP_STATUS_CONNECTION_FAILURE       0x01
#define CIP_STATUS_RESOURCE_UNAVAILABLE     0x02
#define CIP_STATUS_INVALID_PARAMETER        0x03
#define CIP_STATUS_PATH_SEGMENT_ERROR       0x04
#define CIP_STATUS_PATH_DESTINATION_UNKNOWN 0x05
#define CIP_STATUS_PARTIAL_TRANSFER         0x06
#define CIP_STATUS_CONNECTION_LOST          0x07
#define CIP_STATUS_SERVICE_NOT_SUPPORTED    0x08
#define CIP_STATUS_INVALID_ATTRIBUTE        0x09
#define CIP_STATUS_ATTRIBUTE_LIST_ERROR     0x0A
#define CIP_STATUS_ALREADY_IN_REQUESTED_MODE 0x0B
#define CIP_STATUS_OBJECT_STATE_CONFLICT    0x0C
#define CIP_STATUS_OBJECT_ALREADY_EXISTS    0x0D
#define CIP_STATUS_ATTRIBUTE_NOT_SETTABLE   0x0E
#define CIP_STATUS_PRIVILEGE_VIOLATION      0x0F
#define CIP_STATUS_DEVICE_STATE_CONFLICT    0x10
#define CIP_STATUS_REPLY_DATA_TOO_LARGE     0x11
#define CIP_STATUS_FRAGMENTATION_OF_PRIMITIVE_VALUE 0x12
#define CIP_STATUS_NOT_ENOUGH_DATA          0x13
#define CIP_STATUS_ATTRIBUTE_NOT_SUPPORTED  0x14
#define CIP_STATUS_TOO_MUCH_DATA            0x15
#define CIP_STATUS_OBJECT_DOES_NOT_EXIST    0x16
#define CIP_STATUS_SERVICE_FRAGMENTATION_SEQUENCE_NOT_IN_PROGRESS 0x17
#define CIP_STATUS_NO_STORED_ATTRIBUTE_DATA 0x18
#define CIP_STATUS_STORE_OPERATION_FAILURE  0x19
#define CIP_STATUS_ROUTING_FAILURE          0x1A
#define CIP_STATUS_ROUTING_FAILURE_REQUEST_PACKET_TOO_LARGE 0x1B
#define CIP_STATUS_ROUTING_FAILURE_RESPONSE_PACKET_TOO_LARGE 0x1C
#define CIP_STATUS_MISSING_ATTRIBUTE_LIST_ENTRY_DATA 0x1D
#define CIP_STATUS_INVALID_ATTRIBUTE_VALUE_LIST 0x1E
#define CIP_STATUS_EMBEDDED_SERVICE_ERROR   0x1F
#define CIP_STATUS_VENDOR_SPECIFIC_ERROR    0x20
#define CIP_STATUS_INVALID_PARAMETER        0x21
#define CIP_STATUS_WRITE_ONCE_VALUE_OR_MEDIUM_ALREADY_WRITTEN 0x22
#define CIP_STATUS_INVALID_REPLY_RECEIVED   0x23
#define CIP_STATUS_BUFFER_OVERFLOW          0x24
#define CIP_STATUS_INVALID_MESSAGE_FORMAT   0x25
#define CIP_STATUS_KEY_FAILURE_IN_PATH      0x26
#define CIP_STATUS_PATH_SIZE_INVALID        0x27
#define CIP_STATUS_UNEXPECTED_ATTRIBUTE_IN_LIST 0x28
#define CIP_STATUS_INVALID_MEMBER_ID        0x29
#define CIP_STATUS_MEMBER_NOT_SETTABLE      0x2A
#define CIP_STATUS_GROUP2_ONLY_SERVER_GENERAL_FAILURE 0x2B
#define CIP_STATUS_UNKNOWN_MODBUS_ERROR     0x2C
#define CIP_STATUS_ATTRIBUTE_NOT_GETTABLE   0x2D
```

### IOI (Input/Output Image) Path Construction

```c
/**
 * IOI Path Segment Types
 */
#define IOI_SEGMENT_PORT                0x00  // Port segment
#define IOI_SEGMENT_LOGICAL             0x20  // Logical segment
#define IOI_SEGMENT_NETWORK             0x40  // Network segment
#define IOI_SEGMENT_SYMBOLIC            0x60  // Symbolic segment
#define IOI_SEGMENT_DATA                0x80  // Data segment
#define IOI_SEGMENT_CONSTRUCTED_DATA    0xA0  // Constructed data segment
#define IOI_SEGMENT_ELECTRONIC_KEY      0xC0  // Electronic key segment

/**
 * Logical Segment Format
 */
#define IOI_LOGICAL_CLASS_ID            0x20  // Class ID
#define IOI_LOGICAL_INSTANCE_ID         0x24  // Instance ID (8-bit)
#define IOI_LOGICAL_INSTANCE_ID_16      0x25  // Instance ID (16-bit)
#define IOI_LOGICAL_INSTANCE_ID_32      0x26  // Instance ID (32-bit)
#define IOI_LOGICAL_MEMBER_ID           0x28  // Member ID (8-bit)
#define IOI_LOGICAL_MEMBER_ID_16        0x29  // Member ID (16-bit)
#define IOI_LOGICAL_MEMBER_ID_32        0x2A  // Member ID (32-bit)
#define IOI_LOGICAL_CONNECTION_POINT    0x2C  // Connection point
#define IOI_LOGICAL_ATTRIBUTE_ID        0x30  // Attribute ID (8-bit)
#define IOI_LOGICAL_ATTRIBUTE_ID_16     0x31  // Attribute ID (16-bit)
#define IOI_LOGICAL_ATTRIBUTE_ID_32     0x32  // Attribute ID (32-bit)
#define IOI_LOGICAL_SPECIAL             0x34  // Special
#define IOI_LOGICAL_SERVICE_ID          0x38  // Service ID

/**
 * Symbolic Segment Format
 */
#define IOI_SYMBOLIC_ASCII              0x91  // ASCII symbolic segment
#define IOI_SYMBOLIC_EXTENDED_STRING    0x92  // Extended string symbolic segment
#define IOI_SYMBOLIC_NUMERIC            0x93  // Numeric symbolic segment
#define IOI_SYMBOLIC_EXTENDED_NUMERIC   0x94  // Extended numeric symbolic segment

/**
 * Data Segment Types
 */
#define IOI_DATA_SIMPLE                 0x80  // Simple data segment
#define IOI_DATA_ANSI_EXTENDED_SYMBOL   0x91  // ANSI Extended Symbol segment
```

### Tag Read/Write Operations

```c
/**
 * Read Tag Service (0x4C)
 */
typedef struct __attribute__((packed)) {
    uint8_t  service;           // 0x4C
    uint8_t  path_size;         // Path size in words
    uint8_t  path[];            // IOI path to tag
    uint16_t element_count;     // Number of elements to read
} read_tag_request_t;

/**
 * Write Tag Service (0x4D)
 */
typedef struct __attribute__((packed)) {
    uint8_t  service;           // 0x4D
    uint8_t  path_size;         // Path size in words
    uint8_t  path[];            // IOI path to tag
    uint16_t data_type;         // CIP data type
    uint16_t element_count;     // Number of elements to write
    uint8_t  data[];            // Tag data
} write_tag_request_t;

/**
 * Read Tag Fragmented Service (0x52)
 */
typedef struct __attribute__((packed)) {
    uint8_t  service;           // 0x52
    uint8_t  path_size;         // Path size in words
    uint8_t  path[];            // IOI path to tag
    uint16_t element_count;     // Number of elements to read
    uint32_t byte_offset;       // Byte offset for continuation
} read_tag_fragmented_request_t;

/**
 * Write Tag Fragmented Service (0x53)
 */
typedef struct __attribute__((packed)) {
    uint8_t  service;           // 0x53
    uint8_t  path_size;         // Path size in words
    uint8_t  path[];            // IOI path to tag
    uint16_t data_type;         // CIP data type
    uint16_t element_count;     // Number of elements to write
    uint32_t byte_offset;       // Byte offset for continuation
    uint8_t  data[];            // Tag data fragment
} write_tag_fragmented_request_t;
```

## Connection Management

### Forward Open Request

```c
/**
 * Forward Open Request (Service 0x54)
 */
typedef struct __attribute__((packed)) {
    uint8_t  service;                   // 0x54
    uint8_t  path_size;                 // Path size in words
    uint8_t  path[6];                   // Path to Connection Manager
    uint8_t  priority_time_tick;        // Priority/Time tick
    uint8_t  timeout_ticks;             // Timeout ticks
    uint32_t o_to_t_connection_id;      // O->T connection ID
    uint32_t t_to_o_connection_id;      // T->O connection ID
    uint16_t connection_serial_number;  // Connection serial number
    uint16_t originator_vendor_id;      // Originator vendor ID
    uint32_t originator_serial_number;  // Originator serial number
    uint8_t  connection_timeout_multiplier; // Connection timeout multiplier
    uint8_t  reserved[3];               // Reserved
    uint32_t o_to_t_rpi;                // O->T RPI (Request Packet Interval)
    uint16_t o_to_t_connection_parameters; // O->T connection parameters
    uint32_t t_to_o_rpi;                // T->O RPI
    uint16_t t_to_o_connection_parameters; // T->O connection parameters
    uint8_t  transport_type_trigger;    // Transport type/trigger
    uint8_t  connection_path_size;      // Connection path size
    uint8_t  connection_path[];         // Connection path
} forward_open_request_t;

/**
 * Large Forward Open Request (Service 0x5B)
 */
typedef struct __attribute__((packed)) {
    uint8_t  service;                   // 0x5B
    uint8_t  path_size;                 // Path size in words
    uint8_t  path[6];                   // Path to Connection Manager
    uint8_t  priority_time_tick;        // Priority/Time tick
    uint8_t  timeout_ticks;             // Timeout ticks
    uint32_t o_to_t_connection_id;      // O->T connection ID
    uint32_t t_to_o_connection_id;      // T->O connection ID
    uint16_t connection_serial_number;  // Connection serial number
    uint16_t originator_vendor_id;      // Originator vendor ID
    uint32_t originator_serial_number;  // Originator serial number
    uint8_t  connection_timeout_multiplier; // Connection timeout multiplier
    uint8_t  reserved[3];               // Reserved
    uint32_t o_to_t_rpi;                // O->T RPI
    uint32_t o_to_t_connection_parameters; // O->T connection parameters (32-bit)
    uint32_t t_to_o_rpi;                // T->O RPI
    uint32_t t_to_o_connection_parameters; // T->O connection parameters (32-bit)
    uint8_t  transport_type_trigger;    // Transport type/trigger
    uint8_t  connection_path_size;      // Connection path size
    uint8_t  connection_path[];         // Connection path
} large_forward_open_request_t;
```

### Forward Close Request

```c
/**
 * Forward Close Request (Service 0x4E)
 */
typedef struct __attribute__((packed)) {
    uint8_t  service;                   // 0x4E
    uint8_t  path_size;                 // Path size in words
    uint8_t  path[6];                   // Path to Connection Manager
    uint8_t  priority_time_tick;        // Priority/Time tick
    uint8_t  timeout_ticks;             // Timeout ticks
    uint16_t connection_serial_number;  // Connection serial number
    uint16_t originator_vendor_id;      // Originator vendor ID
    uint32_t originator_serial_number;  // Originator serial number
    uint8_t  connection_path_size;      // Connection path size
    uint8_t  reserved;                  // Reserved
    uint8_t  connection_path[];         // Connection path
} forward_close_request_t;
```

## CIP Data Types

### Standard CIP Data Types

```c
/**
 * CIP Data Type Codes
 */
#define CIP_TYPE_BOOL           0xC1    // Boolean
#define CIP_TYPE_SINT           0xC2    // Signed 8-bit integer
#define CIP_TYPE_INT            0xC3    // Signed 16-bit integer
#define CIP_TYPE_DINT           0xC4    // Signed 32-bit integer
#define CIP_TYPE_LINT           0xC5    // Signed 64-bit integer
#define CIP_TYPE_USINT          0xC6    // Unsigned 8-bit integer
#define CIP_TYPE_UINT           0xC7    // Unsigned 16-bit integer
#define CIP_TYPE_UDINT          0xC8    // Unsigned 32-bit integer
#define CIP_TYPE_ULINT          0xC9    // Unsigned 64-bit integer
#define CIP_TYPE_REAL           0xCA    // 32-bit floating point
#define CIP_TYPE_LREAL          0xCB    // 64-bit floating point
#define CIP_TYPE_STIME          0xCC    // Synchronized time
#define CIP_TYPE_DATE           0xCD    // Date
#define CIP_TYPE_TIME_OF_DAY    0xCE    // Time of day
#define CIP_TYPE_DATE_AND_TIME  0xCF    // Date and time
#define CIP_TYPE_STRING         0xD0    // Character string
#define CIP_TYPE_BYTE           0xD1    // Byte string
#define CIP_TYPE_WORD           0xD2    // Word string
#define CIP_TYPE_DWORD          0xD3    // Double word string
#define CIP_TYPE_LWORD          0xD4    // Long word string
#define CIP_TYPE_STRING2        0xD5    // Character string 2
#define CIP_TYPE_FTIME          0xD6    // Duration
#define CIP_TYPE_LTIME          0xD7    // Long duration
#define CIP_TYPE_ITIME          0xD8    // Intermediate time
#define CIP_TYPE_STRINGN        0xD9    // Character string N
#define CIP_TYPE_SHORT_STRING   0xDA    // Short string
#define CIP_TYPE_TIME           0xDB    // Time
#define CIP_TYPE_EPATH          0xDC    // Electronic path
#define CIP_TYPE_ENGUNIT        0xDD    // Engineering units
#define CIP_TYPE_STRINGI        0xDE    // International character string

/**
 * Constructed Data Types
 */
#define CIP_TYPE_ARRAY          0xA0    // Array
#define CIP_TYPE_STRUCT         0xA1    // Structure
#define CIP_TYPE_UNION          0xA2    // Union
```

### Multi-Service Request

```c
/**
 * Multi-Service Request (Service 0x0A)
 */
typedef struct __attribute__((packed)) {
    uint8_t  service;           // 0x0A
    uint8_t  path_size;         // Path size in words
    uint8_t  path[];            // IOI path (usually to Message Router)
    uint16_t service_count;     // Number of services
    uint16_t service_offsets[]; // Array of service offsets
    // Individual service requests follow
} multi_service_request_t;
```

## Vendor-Specific Extensions

### Rockwell Automation Extensions

**Symbol Object (Class 0x6B) - Tag Enumeration:**
```c
/**
 * Get Instance Attribute List (Service 0x55)
 */
typedef struct __attribute__((packed)) {
    uint8_t  service;           // 0x55
    uint8_t  path_size;         // Path size in words
    uint8_t  path[6];           // Path to Symbol Object
    uint16_t attribute_count;   // Number of attributes
    uint16_t attribute_list[];  // List of attribute IDs
} get_instance_attribute_list_request_t;
```

**Template Object (Class 0x6C) - UDT Definitions:**
```c
/**
 * Get Attribute List (Service 0x03) - UDT Template
 */
typedef struct __attribute__((packed)) {
    uint8_t  service;           // 0x03
    uint8_t  path_size;         // Path size in words
    uint8_t  path[6];           // Path to Template Object
    uint16_t attribute_count;   // Number of attributes
    uint16_t attribute_list[];  // List of attribute IDs
} get_attribute_list_request_t;
```

### OMRON Extensions

**Simple Data Segment (Data Type 0x80):**
```c
/**
 * OMRON Simple Data Segment for Large Data Transfer
 */
typedef struct __attribute__((packed)) {
    uint8_t  segment_type;      // 0x80
    uint8_t  segment_length;    // 0x03 (in words)
    uint32_t byte_offset;       // Byte offset
    uint16_t transfer_size;     // Transfer size in bytes
} omron_simple_data_segment_t;
```

**Tag Name Server Object (Class 0x6A):**
```c
/**
 * Get Instance List Extended 2 (Service 0x5F)
 */
typedef struct __attribute__((packed)) {
    uint8_t  service;           // 0x5F
    uint8_t  path_size;         // Path size in words
    uint8_t  path[6];           // Path to Tag Name Server
    uint32_t start_instance;    // Starting instance ID
    uint32_t instance_count;    // Number of instances
    uint16_t variable_kind;     // Variable kind (1=system, 2=user)
} get_instance_list_extended2_request_t;
```

## Error Handling and Status Codes

### Fragmentation Handling

Status code 0x06 (Partial Transfer) indicates that data transfer requires fragmentation:

```c
/**
 * Fragmentation State Management
 */
typedef enum {
    FRAG_STATE_NONE = 0,        // No fragmentation
    FRAG_STATE_FIRST_FRAGMENT,  // First fragment received
    FRAG_STATE_MIDDLE_FRAGMENT, // Middle fragment received
    FRAG_STATE_LAST_FRAGMENT    // Last fragment received
} fragmentation_state_t;

typedef struct {
    fragmentation_state_t state;
    uint32_t total_size;        // Total expected size
    uint32_t current_offset;    // Current byte offset
    uint8_t  *buffer;           // Assembly buffer
    size_t   buffer_size;       // Buffer size
} fragmentation_context_t;
```

### Connection State Management

```c
/**
 * Connection States
 */
typedef enum {
    CONN_STATE_UNCONNECTED = 0,
    CONN_STATE_REGISTERED,      // Session registered
    CONN_STATE_CONNECTED,       // Connection established
    CONN_STATE_TIMED_OUT,       // Connection timed out
    CONN_STATE_ERROR            // Connection error
} connection_state_t;

typedef struct {
    connection_state_t state;
    uint32_t session_handle;
    uint32_t connection_id;
    uint16_t connection_serial;
    uint16_t connection_size;
    time_t   last_activity;
    uint32_t timeout_ms;
} connection_context_t;
```

## Implementation Guidelines

### Protocol Flow Best Practices

1. **Session Management**: Always register session before attempting operations
2. **Connection Sizing**: Try large connections first, fallback to smaller sizes
3. **Error Handling**: Implement proper status code handling and recovery
4. **Fragmentation**: Support fragmented transfers for large data
5. **Timeouts**: Implement appropriate timeouts and keep-alive mechanisms
6. **Context Tracking**: Use sender context for request/response correlation

### Performance Considerations

- **Batch Operations**: Use multi-service requests where supported
- **Connection Reuse**: Maintain connections across multiple operations
- **Optimal Packet Sizing**: Balance throughput with compatibility
- **Cache Management**: Cache UDT definitions and tag metadata
- **Network Discovery**: Implement efficient broadcast-based discovery

### Security Considerations

- **Authentication**: Implement proper authentication where required
- **Authorization**: Respect device access controls
- **Encryption**: Use secure connections where supported
- **Audit Logging**: Log all configuration changes and critical operations