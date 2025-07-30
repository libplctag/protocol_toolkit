# Modbus TCP Server Example

This example demonstrates how to build a full-featured Modbus TCP server using the Protocol Toolkit library. It showcases advanced features including:

- **Multiple State Machines**: One server state machine plus individual client state machines
- **Multiple Transition Tables**: Different state tables for connection management and protocol handling
- **Zero-Allocation Design**: All memory statically allocated at startup
- **Complete Modbus Implementation**: Supports multiple function codes with proper error handling

## Architecture Overview

### State Machine Design

The example uses a hierarchical state machine architecture:

1. **Server State Machine**
   - Handles connection acceptance
   - Manages server-level events (heartbeat, etc.)
   - Uses 1 transition table with server-specific states

2. **Client State Machines** (up to 10 concurrent)
   - Each client connection gets its own state machine
   - Uses 2 transition tables per client:
     - **Connection Table**: Manages connection lifecycle states
     - **Protocol Table**: Handles Modbus protocol states

### Register Storage

The server implements all standard Modbus register types:

- **100 Coils** (discrete outputs) - read/write bits
- **100 Discrete Inputs** - read-only bits
- **100 Holding Registers** - read/write 16-bit values
- **100 Input Registers** - read-only 16-bit values

### Supported Modbus Functions

- **0x01**: Read Coils
- **0x02**: Read Discrete Inputs
- **0x03**: Read Holding Registers
- **0x04**: Read Input Registers
- **0x05**: Write Single Coil
- **0x06**: Write Single Register
- **0x0F**: Write Multiple Coils
- **0x10**: Write Multiple Registers

## Building and Running

### Build the Examples

```bash
cd src/build
cmake ..
make
```

### Start the Server

```bash
./bin/examples/modbus_tcp_server
```

The server will start on port **5020** and display:

```
Starting Modbus TCP Server on port 5020
Register configuration: 100 coils, 100 discrete inputs, 100 holding registers, 100 input registers
Server listening on port 5020
Demonstrating multiple state machines:
- 1 server state machine (accept connections, heartbeat)
- Up to 10 client state machines (per connection)
- Each client has 2 transition tables (connection + protocol states)
- Server has 1 transition table (server states)

Press Ctrl+C to stop

[Server] Heartbeat - 0 active clients, registers: coils[0]=ON, holding[0]=12345
```

### Test with the Included Client

```bash
./bin/examples/modbus_test_client
```

This will run a comprehensive test suite demonstrating all supported functions:

```
=== Test 1: Read Coils (0x01) ===
Response: TID=1, Protocol=0, Length=3, Unit=1
Function: 0x01, Byte count: 2
Coil values: 1 0 0 0 0 1 0 0 0 0

=== Test 2: Read Holding Registers (0x03) ===
Response: TID=2, Protocol=0, Length=12, Unit=1
Function: 0x03, Byte count: 10
Register values: 12345 54321 0 0 0

=== Test 3: Write Single Coil (0x05) ===
Response: TID=3, Protocol=0, Length=6, Unit=1
Function: 0x05, Address: 10, Value: 0xFF00
Coil 10 set to ON
```

## Code Structure Highlights

### Multiple Transition Tables

Each client uses two separate transition tables:

```c
// Connection lifecycle transitions
ptk_tt_add_transition(&client->connection_table, CLIENT_STATE_CONNECTED, EVENT_CLIENT_READ,
                     CLIENT_STATE_READING_HEADER, NULL, client_read_action);

// Protocol handling transitions
ptk_tt_add_transition(&client->protocol_table, CLIENT_STATE_SENDING_RESPONSE, EVENT_CLIENT_WRITE,
                     CLIENT_STATE_READING_HEADER, NULL, client_write_action);
```

### State Machine Hierarchy

```c
// Server state machine manages connections
ptk_sm_init(&server->server_state_machine, server->server_tables, 1,
            server->server_sources, 2, loop, server);

// Each client gets its own state machine
ptk_sm_init(&client->state_machine, client->tables, 2, client->sources, 3, loop, client);
```

### Zero-Allocation Design

All memory is allocated statically at startup:

```c
// Pre-allocated client contexts
client_context_t clients[MAX_CLIENTS];

// Pre-allocated transition arrays
ptk_transition_t connection_transitions[10];
ptk_transition_t protocol_transitions[15];

// Pre-allocated event source arrays
ptk_event_source_t *sources[3];
```

## Testing with Other Clients

You can test with any Modbus TCP client. Popular options:

### Using modpoll (if available)

```bash
# Read 10 coils starting from address 0
modpoll -m tcp -a 1 -r 1 -c 10 -t 0 127.0.0.1

# Read 5 holding registers starting from address 0
modpoll -m tcp -a 1 -r 1 -c 5 -t 4 127.0.0.1

# Write value 1234 to holding register 10
modpoll -m tcp -a 1 -r 11 -t 4 127.0.0.1 1234
```

### Using Python with pymodbus

```python
from pymodbus.client import ModbusTcpClient

client = ModbusTcpClient('127.0.0.1', port=5020)
client.connect()

# Read coils
result = client.read_coils(0, 10, slave=1)
print(f"Coils: {result.bits}")

# Read holding registers
result = client.read_holding_registers(0, 5, slave=1)
print(f"Registers: {result.registers}")

# Write single register
client.write_register(10, 1234, slave=1)

client.close()
```

## Learning Points

This example demonstrates several important Protocol Toolkit concepts:

1. **Multiple State Machines**: How to manage multiple concurrent state machines in a single event loop

2. **Transition Table Separation**: Using multiple transition tables to organize different aspects of state management

3. **Static Memory Management**: Zero-allocation design suitable for embedded or resource-constrained environments

4. **Event-Driven Architecture**: How I/O events trigger state transitions and actions

5. **Protocol Implementation**: Building a complete industrial protocol on top of the toolkit

6. **Error Handling**: Proper exception handling and client connection management

The server can handle up to 10 concurrent Modbus TCP clients, each with their own state machine and protocol context, demonstrating the scalability of the event-driven approach.
