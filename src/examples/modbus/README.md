# Modbus TCP Tools

This directory contains minimal Modbus TCP client and server implementations using only the protocol toolkit APIs.

## Features

### Modbus Client (`modbus_client`)
- **All Modbus Functions**: Read/write coils, discrete inputs, holding registers, input registers
- **Command Line Interface**: Specify operations via command line arguments
- **Error Handling**: Proper exception code reporting and timeout management
- **Minimal Dependencies**: Uses only protocol toolkit APIs

### Modbus Server (`modbus_server`)
- **Multi-threaded**: One thread per client connection
- **Configurable Register Counts**: Specify number of each register type
- **Statistics**: Optional real-time statistics display
- **Thread-safe**: Proper synchronization for concurrent access

## Building

```bash
cd /path/to/protocol_toolkit
mkdir build && cd build
cmake ..
make modbus_client modbus_server
```

## Usage

### Modbus Client

```bash
# Basic usage
./modbus_client <host> <command> <args...>

# Examples
./modbus_client 192.168.1.100 read-holding 40001 10
./modbus_client 192.168.1.100 write-reg 40001 1234
./modbus_client 192.168.1.100 read-coils 1 16
./modbus_client 192.168.1.100 write-coil 1 1

# With options
./modbus_client -p 1502 -u 2 -t 10000 192.168.1.100 read-holding 40001 5
```

#### Client Commands

- `read-coils <start> <count>` - Read coils (0x addresses)
- `read-discrete <start> <count>` - Read discrete inputs (1x addresses) 
- `read-holding <start> <count>` - Read holding registers (4x addresses)
- `read-input <start> <count>` - Read input registers (3x addresses)
- `write-coil <addr> <0|1>` - Write single coil
- `write-reg <addr> <value>` - Write single register
- `write-coils <start> <0|1> [0|1...]` - Write multiple coils
- `write-regs <start> <value> [value...]` - Write multiple registers

#### Client Options

- `-p <port>` - TCP port (default: 502)
- `-u <unit_id>` - Unit ID (default: 1)  
- `-t <timeout>` - Timeout in ms (default: 5000)

### Modbus Server

```bash
# Basic usage (defaults: port 502, 1000 of each register type)
./modbus_server

# Custom configuration
./modbus_server -p 1502 -r 5000 -c 2000 -m 20

# With statistics
./modbus_server -s -S 2000
```

#### Server Options

- `-h <host>` - Bind host (default: all interfaces)
- `-p <port>` - Bind port (default: 502)
- `-c <count>` - Number of coils (default: 1000)
- `-d <count>` - Number of discrete inputs (default: 1000)
- `-i <count>` - Number of input registers (default: 1000)
- `-r <count>` - Number of holding registers (default: 1000)
- `-m <count>` - Maximum clients (default: 10)
- `-t <timeout>` - Client timeout in ms (default: 30000)
- `-s` - Show statistics periodically
- `-S <interval>` - Statistics interval in ms (default: 5000)

## Testing

Start a server:
```bash
./modbus_server -s
```

Test with the client:
```bash
# Read some test data
./modbus_client localhost read-holding 0 10
./modbus_client localhost read-coils 0 20

# Write some data
./modbus_client localhost write-reg 100 9999
./modbus_client localhost write-coil 50 1

# Verify the writes
./modbus_client localhost read-holding 100 1
./modbus_client localhost read-coils 50 1
```

## Protocol Details

- **Modbus TCP/IP** with standard MBAP header
- **Unit ID support** for multi-device simulation
- **Exception handling** with proper error codes
- **Transaction ID** management for request/response matching
- **Thread-safe** server implementation
- **Configurable timeouts** and limits

## Implementation Notes

- Uses only existing protocol toolkit APIs (`ptk_socket.h`, `ptk_buf.h`, `ptk_thread.h`, etc.)
- Synchronous socket operations throughout
- No complex event loops or async handling
- Minimal memory allocation and predictable resource usage
- Clean error handling with proper cleanup