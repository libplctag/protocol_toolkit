# Modbus TCP Server

A multi-connection Modbus TCP server implemented in C using libuv and the generated Modbus codec functions.

## Features

- **Multi-connection support**: Handles multiple simultaneous client connections
- **Full Modbus function support**: Implements 8 standard Modbus functions
- **Generated codecs**: Uses automatically generated encoding/decoding functions from `modbus.def`
- **10 registers/coils each**: Supports 10 coils, discrete inputs, holding registers, and input registers

## Supported Modbus Functions

| Function Code | Function Name | Description |
|---------------|---------------|-------------|
| 0x01 | Read Coils | Read 1-10 coil values (binary outputs) |
| 0x02 | Read Discrete Inputs | Read 1-10 discrete input values (binary inputs) |
| 0x03 | Read Holding Registers | Read 1-10 holding register values (read/write 16-bit) |
| 0x04 | Read Input Registers | Read 1-10 input register values (read-only 16-bit) |
| 0x05 | Write Single Coil | Write one coil value |
| 0x06 | Write Single Register | Write one holding register value |
| 0x0F | Write Multiple Coils | Write multiple coil values |
| 0x10 | Write Multiple Registers | Write multiple holding register values |

## Data Model

The server maintains:
- **Coils[0-9]**: Read/write binary outputs (initially alternating 0/1)
- **Discrete Inputs[0-9]**: Read-only binary inputs (initially alternating 1/0)
- **Holding Registers[0-9]**: Read/write 16-bit values (initially 1000-1009)
- **Input Registers[0-9]**: Read-only 16-bit values (initially 2000-2009)

## Building

```bash
# Install libuv (macOS with Homebrew)
brew install libuv

# Build the server
make modbus_server
```

## Running

```bash
# Start the server (listens on port 502)
./modbus_server

# The server will output:
# [timestamp] INFO main:116 - Modbus TCP server listening on port 502
```

## Testing

You can test the server using any Modbus TCP client, such as:

- `mbpoll` (command line)
- `qmodmaster` (GUI)
- `modbus-cli` (Node.js)
- Python `pymodbus` library

Example with mbpoll:
```bash
# Read 5 holding registers starting at address 0
mbpoll -t 4 -r 1 -c 5 localhost

# Write value 1234 to holding register 0
mbpoll -t 4 -r 1 -c 1 -1 1234 localhost
```

## Architecture

The server uses:
- **libuv**: Asynchronous I/O for handling multiple connections
- **Generated codecs**: Auto-generated from `modbus.def` using `gen_python.py`
- **MBAP protocol**: Standard Modbus TCP encapsulation
- **User-defined functions**: Custom implementations for variable-length data fields

## Files

- `modbus_server.c`: Main server implementation
- `modbus.def`: Modbus protocol definitions
- `modbus.h/modbus.c`: Generated codec functions
- `log.h/log.c`: Logging implementation
- `buf.h`: Buffer utility functions