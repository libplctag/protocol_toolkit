# Modbus TCP Client

A command-line Modbus TCP client that supports reading and writing registers, coils, and discrete inputs.

## Building

The client is built as part of the main project build:

```bash
mkdir build && cd build
cmake ..
make
```

The binary will be created at `build/bin/modbus_client`.

## Usage

### Basic Syntax

```bash
modbus_client [OPTIONS] OPERATION
```

### Connection Options

- `-h, --host=HOST` - Modbus server host (default: 127.0.0.1)
- `-p, --port=PORT` - Modbus server port (default: 502)
- `-u, --unit-id=ID` - Unit ID (default: 1)
- `-v, --verbose` - Enable verbose output
- `--help` - Show help message

### Read Operations

- `--read-holding=ADDR[,COUNT]` - Read holding register(s)
- `--read-input=ADDR[,COUNT]` - Read input register(s)
- `--read-coils=ADDR[,COUNT]` - Read coil(s)
- `--read-discrete=ADDR[,COUNT]` - Read discrete input(s)

### Write Operations

- `--write-holding=ADDR,VALUE` - Write single holding register
- `--write-coil=ADDR,VALUE` - Write single coil (0 or 1)
- `--write-holdings=ADDR,VAL1,VAL2,...` - Write multiple holding registers
- `--write-coils=ADDR,VAL1,VAL2,...` - Write multiple coils (0 or 1)

## Examples

### Read Operations

```bash
# Read holding register 100
./modbus_client --read-holding=100

# Read 10 holding registers starting at address 100
./modbus_client --read-holding=100,10

# Read input register 50
./modbus_client --read-input=50

# Read 5 coils starting at address 0
./modbus_client --read-coils=0,5

# Read discrete inputs with verbose output
./modbus_client -v --read-discrete=10,8
```

### Write Operations

```bash
# Write value 1234 to holding register 100
./modbus_client --write-holding=100,1234

# Write multiple values to holding registers
./modbus_client --write-holdings=100,1000,2000,3000

# Turn on coil 5
./modbus_client --write-coil=5,1

# Set multiple coils
./modbus_client --write-coils=0,1,0,1,1
```

### Remote Server

```bash
# Connect to remote server
./modbus_client -h 192.168.1.100 -p 502 --read-holding=100

# Use different unit ID
./modbus_client -h 192.168.1.100 -u 2 --read-input=50,10
```

## Output Format

### Read Operations

The client displays the values in both decimal and hexadecimal format:

```
Holding register 100: 1234 (0x04D2)
```

For multiple values:

```
Holding registers starting at 100:
  [100]: 1000 (0x03E8)
  [101]: 2000 (0x07D0)
  [102]: 3000 (0x0BB8)
```

### Write Operations

The client confirms successful writes:

```
Successfully wrote 1234 to holding register 100
Successfully wrote 3 values starting at holding register 100
```

## Error Handling

The client provides clear error messages for common issues:

- Connection failures
- Invalid register addresses
- Modbus exceptions
- Invalid command line arguments

Use the `-v` flag for more detailed error information.
