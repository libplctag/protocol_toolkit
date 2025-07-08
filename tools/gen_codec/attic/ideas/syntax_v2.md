I want to be able to define virtual fields.  These are fields that are based on concrete fields but with some sort of expression that generates their value or sets their value.

These are particularly important for bit fields so that they can be decoded and encoded from bool values.

The generator would make C structs with the virtual fields that could be accessed directly via pointers to the struct.

The underlying data that was used for the virtual field could be marked as hidden.

A possible new syntax for PDL is shown below with a partial example from EtherNet/IP.

## Import Syntax

```pdl
// Import external definitions
import eip_common = "eip_common.pdl"
import modbus_types = "modbus/types.pdl"

// Use imported definitions via namespace
def my_message = {
    type: message,
    fields: [
        header: eip_common.eip_header,           // Use imported type
        modbus_data: modbus_types.holding_reg,   // Use imported type
        local_field: u32_le                      // Local type
    ]
}
```

## Example

```pdl
// Type definitions with byte ordering
def u16_le = { type: u16, byte_order: [0, 1]}
def u32_le = { type: u32, byte_order: [0, 1, 2, 3]}
def u64_le = { type: u64, byte_order: [0, 1, 2, 3, 4, 5, 6, 7]}

// Constants
def EIP_SEND_RR_DATA = { type: u16_le, const: 0x006F}
def EIP_REGISTER_SESSION = { type: u16_le, const: 0x0065}
def EIP_UNREGISTER_SESSION = { type: u16_le, const: 0x0066}

def eip_encap_header = {
    type: message,
    fields: [
        command: u16_le,
        length: u16_le,
        session_handle: u32_le,
        status: u32_le,
        sender_context: u64_le,
        options: u32_le,
    ]
}

def eip_send_rr_data_req = {
    type: message,
    match: ($.header.command == EIP_SEND_RR_DATA),
    fields: [
        interface_handle: u32,    // Interface handle
        router_timeout: u16,      // Router timeout
        item_count: u16           // Number of items (always 2)
    ]
}

def eip_register_session_req = {
    type: message,
    match: ($.header.command == EIP_REGISTER_SESSION),
    fields: [
        version: { type: u16_le, const: 1 },
        options: { type: u16_le, const: 0 }
    ]
}

def eip_unregister_session_req =  {
    type: message,
    match: ($.header.session_handle != 0 && $.header.command == EIP_UNREGISTER_SESSION),
}
```

Showing variant matching:

```pdl
def eip_message = {
    type: message,
    fields: [
        header: eip_encap_header,
        payload: [
            eip_send_rr_data_req,
            eip_register_session_req,
            eip_unregister_session_req
        ]
    ]
}
```

The generator will generate a struct with direct field access for all visible fields.

```pdl
def eip_header = {
    type: message,
    fields: [
        command: u16_le,
        length: u16_le,
        session_handle: u32_le,
        status: u32_le,
        sender_context: u64_le,
        options: u32_le
    ]
}
```

This will result in the following code in the .h file:

```c
/* message types */
typedef enum {
    /* ... */
    EIP_HEADER_TYPE = 1,
    /* ... */
} message_type_t;

/** definitions for message eip_header */

/* struct definition with direct field access */
typedef struct eip_header_t {
    const int message_type;              // EIP_HEADER_TYPE
    u16 command;
    u16 length;
    u32 session_handle;
    u32 status;
    u64 sender_context;
    u32 options;
} eip_header_t;

/* constructor/destructor */
ptk_err eip_header_create(ptk_allocator_t *alloc, eip_header_t **header);
void eip_header_dispose(ptk_allocator_t *alloc, eip_header_t *header);

/* encode/decode functions */
ptk_err eip_header_encode(ptk_allocator_t *alloc, ptk_buf_t *buf, const eip_header_t *header);
ptk_err eip_header_decode(ptk_allocator_t *alloc, eip_header_t **header, ptk_buf_t *buf);
```

## Direct Struct Access with Variable-Length Support

With direct struct access, variable-length fields are handled as array pointers.

```pdl
def variable_message = {
    type: message,
    fields: [
        header_size: u16,
        fixed_field: u32,
        name: {
            type: u8[],
            length: {
                decode: (header_size - 6),  // Total header minus fixed fields
                encode: (name.count)
            }
        },
        trailing_field: u16
    ]
}
```

**Generated Structure:**
```c
typedef struct variable_message_t {
    const int message_type;
    u16 header_size;
    u32 fixed_field;
    u8_array_t *name;                   // Variable-length array
    u16 trailing_field;
} variable_message_t;

// Constructor/destructor
ptk_err variable_message_create(ptk_allocator_t *alloc, variable_message_t **msg);
void variable_message_dispose(ptk_allocator_t *alloc, variable_message_t *msg);

// Encode/decode
ptk_err variable_message_encode(ptk_allocator_t *alloc, ptk_buf_t *buf, const variable_message_t *msg);
ptk_err variable_message_decode(ptk_allocator_t *alloc, variable_message_t **msg, ptk_buf_t *buf);

// Safe array accessors for name field
ptk_err variable_message_get_name_element(const variable_message_t *msg, size_t index, u8 *value);
ptk_err variable_message_set_name_element(variable_message_t *msg, size_t index, u8 value);
size_t variable_message_get_name_count(const variable_message_t *msg);
```

## Array Field Patterns

### Simple Arrays (Primitive Types)
```pdl
def modbus_coil_data = {
    type: message,
    fields: [
        byte_count: u8,
        coil_data: u8[byte_count]
    ]
}
```

**Generated Structure:**
```c
typedef struct modbus_coil_data_t {
    const int message_type;
    u8 byte_count;
    u8_array_t *coil_data;              // Safe array of u8 elements
} modbus_coil_data_t;

// Safe array accessors
ptk_err modbus_coil_data_get_coil_data_element(const modbus_coil_data_t *msg, size_t index, u8 *value);
ptk_err modbus_coil_data_set_coil_data_element(modbus_coil_data_t *msg, size_t index, u8 value);
size_t modbus_coil_data_get_coil_data_count(const modbus_coil_data_t *msg);
```

### Aggregate Arrays (Message/Struct Elements)
```pdl
def multi_register_msg = {
    type: message,
    fields: [
        register_count: u16,
        registers: register_info[register_count]
    ]
}
```

**Generated Structure:**
```c
typedef struct multi_register_msg_t {
    const int message_type;
    u16 register_count;
    register_info_array_t *registers;   // Safe array of register_info_t elements
} multi_register_msg_t;

// Safe array accessors
ptk_err multi_register_msg_get_registers_element(const multi_register_msg_t *msg, size_t index, register_info_t *value);
ptk_err multi_register_msg_set_registers_element(multi_register_msg_t *msg, size_t index, const register_info_t *value);
size_t multi_register_msg_get_registers_count(const multi_register_msg_t *msg);
```

### Bit Arrays (Container-Specific)
```pdl
def u8_bit_array = { type: bit[], container_type: u8 }
def u16_bit_array = { type: bit[], container_type: u16_le }
def u32_bit_array = { type: bit[], container_type: u32_le }

def modbus_coils = {
    type: message,
    fields: [
        byte_count: u8,
        coils: u8_bit_array[byte_count]         // u8 containers
    ]
}

def rockwell_tags = {
    type: message,
    fields: [
        dword_count: u16,
        tags: u32_bit_array[dword_count]        // u32 containers
    ]
}
```

**Generated Structures:**
```c
typedef struct modbus_coils_t {
    const int message_type;
    u8 byte_count;
    u8_bit_array_t *coils;                  // u8 container bit array
} modbus_coils_t;

typedef struct rockwell_tags_t {
    const int message_type;
    u16 dword_count;
    u32_bit_array_t *tags;                  // u32 container bit array
} rockwell_tags_t;

// Generated convenience accessors - hide container complexity
// Modbus coils (u8 containers) - bit-level access
static inline ptk_err modbus_coils_get_coils(const modbus_coils_t *msg, size_t bit_index, bool *value) {
    if (!msg || !msg->coils) return PTK_ERR_NULL_PTR;
    return u8_bit_array_safe_get(msg->coils, bit_index, value);
}

static inline ptk_err modbus_coils_set_coils(modbus_coils_t *msg, size_t bit_index, bool value) {
    if (!msg || !msg->coils) return PTK_ERR_NULL_PTR;
    return u8_bit_array_safe_set(msg->coils, bit_index, value);
}

static inline size_t modbus_coils_get_coils_length(const modbus_coils_t *msg) {
    return msg && msg->coils ? u8_bit_array_safe_len(msg->coils) : 0;  // Returns bit count
}

// Modbus coils - container-level access for efficient bulk operations
static inline ptk_err modbus_coils_get_coils_container(const modbus_coils_t *msg, size_t container_index, u8 *value) {
    if (!msg || !msg->coils) return PTK_ERR_NULL_PTR;
    return u8_bit_array_get_container(msg->coils, container_index, value);
}

static inline ptk_err modbus_coils_set_coils_container(modbus_coils_t *msg, size_t container_index, u8 value) {
    if (!msg || !msg->coils) return PTK_ERR_NULL_PTR;
    return u8_bit_array_set_container(msg->coils, container_index, value);
}

static inline size_t modbus_coils_get_coils_container_count(const modbus_coils_t *msg) {
    return msg && msg->coils ? u8_bit_array_container_count(msg->coils) : 0;  // Returns container count
}

// Rockwell tags (u32 containers) - bit-level access
static inline ptk_err rockwell_tags_get_tags(const rockwell_tags_t *msg, size_t bit_index, bool *value) {
    if (!msg || !msg->tags) return PTK_ERR_NULL_PTR;
    return u32_bit_array_safe_get(msg->tags, bit_index, value);
}

static inline ptk_err rockwell_tags_set_tags(rockwell_tags_t *msg, size_t bit_index, bool value) {
    if (!msg || !msg->tags) return PTK_ERR_NULL_PTR;
    return u32_bit_array_safe_set(msg->tags, bit_index, value);
}

static inline size_t rockwell_tags_get_tags_length(const rockwell_tags_t *msg) {
    return msg && msg->tags ? u32_bit_array_safe_len(msg->tags) : 0;  // Returns bit count
}

// Rockwell tags - container-level access for efficient bulk operations
static inline ptk_err rockwell_tags_get_tags_container(const rockwell_tags_t *msg, size_t container_index, u32 *value) {
    if (!msg || !msg->tags) return PTK_ERR_NULL_PTR;
    return u32_bit_array_get_container(msg->tags, container_index, value);
}

static inline ptk_err rockwell_tags_set_tags_container(rockwell_tags_t *msg, size_t container_index, u32 value) {
    if (!msg || !msg->tags) return PTK_ERR_NULL_PTR;
    return u32_bit_array_set_container(msg->tags, container_index, value);
}

static inline size_t rockwell_tags_get_tags_container_count(const rockwell_tags_t *msg) {
    return msg && msg->tags ? u32_bit_array_container_count(msg->tags) : 0;  // Returns container count
}
```

**Usage Examples:**
```c
// Bit-level access with get, set, length pattern
modbus_coils_set_coils(coil_msg, 5, true);                    // Set coil 5
rockwell_tags_set_tags(tag_msg, 10, false);                   // Clear bit 10

bool coil_state;
modbus_coils_get_coils(coil_msg, 5, &coil_state);             // Get coil 5

// Get total bit counts
size_t modbus_bit_count = modbus_coils_get_coils_length(coil_msg);      // Returns bits (e.g., 16)
size_t rockwell_bit_count = rockwell_tags_get_tags_length(tag_msg);     // Returns bits (e.g., 128)

// Container-level access for efficient bulk operations
modbus_coils_set_coils_container(coil_msg, 0, 0xFF);          // Set all 8 coils in byte 0
rockwell_tags_set_tags_container(tag_msg, 1, 0x12345678);     // Set 32 bits at once

u8 coil_byte;
modbus_coils_get_coils_container(coil_msg, 0, &coil_byte);    // Get byte 0 (8 coils)

u32 rockwell_dword;
rockwell_tags_get_tags_container(tag_msg, 1, &rockwell_dword); // Get dword 1 (32 bits)

// Get container counts
size_t modbus_byte_count = modbus_coils_get_coils_container_count(coil_msg);     // Returns containers (e.g., 2)
size_t rockwell_dword_count = rockwell_tags_get_tags_container_count(tag_msg);   // Returns containers (e.g., 4)
```

### Bit Flags (Individual Bit Access)
```pdl
def status_register = {
    type: message,
    fields: [
        _bit_source_do_not_access: u16_le,        // Hidden source field
        ready_flag: { type: bit, source_field: _bit_source_do_not_access, source_bit: 0 },
        error_flag: { type: bit, source_field: _bit_source_do_not_access, source_bit: 1 },
        busy_flag: { type: bit, source_field: _bit_source_do_not_access, source_bit: 2 },
        enabled_flag: { type: bit, source_field: _bit_source_do_not_access, source_bit: 4 }
    ]
}
```

**Generated Structure:**
```c
typedef struct status_register_t {
    const int message_type;
    u16 _bit_source_do_not_access;              // Source field (could be hidden from API)
    bool ready_flag;                            // Virtual bit field
    bool error_flag;                            // Virtual bit field
    bool busy_flag;                             // Virtual bit field
    bool enabled_flag;                          // Virtual bit field
} status_register_t;

// Generated accessors that maintain bit synchronization
static inline bool status_register_get_ready_flag(const status_register_t *msg) {
    return (msg->_bit_source_do_not_access >> 0) & 0x01;
}

static inline void status_register_set_ready_flag(status_register_t *msg, bool value) {
    if (value) {
        msg->_bit_source_do_not_access |= (1 << 0);
    } else {
        msg->_bit_source_do_not_access &= ~(1 << 0);
    }
    msg->ready_flag = value;
}

static inline bool status_register_get_error_flag(const status_register_t *msg) {
    return (msg->_bit_source_do_not_access >> 1) & 0x01;
}

static inline void status_register_set_error_flag(status_register_t *msg, bool value) {
    if (value) {
        msg->_bit_source_do_not_access |= (1 << 1);
    } else {
        msg->_bit_source_do_not_access &= ~(1 << 1);
    }
    msg->error_flag = value;
}

// ... similar for other flags
```

**Usage:**
```c
status_register_t *status;
status_register_create(alloc, &status);

// Direct field access (always synchronized)
if (status->ready_flag && !status->error_flag) {
    // Process ready state
}

// Or use accessor functions for explicit control
status_register_set_enabled_flag(status, true);
bool is_busy = status_register_get_busy_flag(status);
```

### Variant Arrays (Union Types)
```pdl
def cpf_packet = {
    type: message,
    fields: [
        item_count: u16,
        items: [
            null_address_item,
            connected_address_item,
            connected_data_item,
            unconnected_data_item
        ][item_count]
    ]
}
```

**Generated Types and Accessors:**
```c
// Discriminated union for variant array elements
typedef enum {
    CPF_ITEMS_NULL_ADDRESS_ITEM,
    CPF_ITEMS_CONNECTED_ADDRESS_ITEM,
    CPF_ITEMS_CONNECTED_DATA_ITEM,
    CPF_ITEMS_UNCONNECTED_DATA_ITEM
} cpf_items_element_type_t;

typedef struct {
    cpf_items_element_type_t type;
    union {
        null_address_item_t *null_address_item;
        connected_address_item_t *connected_address_item;
        connected_data_item_t *connected_data_item;
        unconnected_data_item_t *unconnected_data_item;
    } element;
} cpf_items_element_t;

typedef struct cpf_packet_vtable_t {
    // ... other functions (all with allocator first parameter) ...

    // Variant array accessors
    ptk_err (*get_items_element)(ptk_allocator_t *alloc, cpf_packet_t *self, size_t index, cpf_items_element_t *element);
    ptk_err (*set_items_element)(ptk_allocator_t *alloc, cpf_packet_t *self, size_t index, const cpf_items_element_t *element);
    ptk_err (*get_items_length)(ptk_allocator_t *alloc, cpf_packet_t *self, size_t *length);

    // Type-specific accessors for convenience
    ptk_err (*get_items_as_null_address)(ptk_allocator_t *alloc, cpf_packet_t *self, size_t index, null_address_item_t **element);
    ptk_err (*get_items_as_connected_address)(ptk_allocator_t *alloc, cpf_packet_t *self, size_t index, connected_address_item_t **element);
    ptk_err (*get_items_as_connected_data)(ptk_allocator_t *alloc, cpf_packet_t *self, size_t index, connected_data_item_t **element);
    ptk_err (*get_items_as_unconnected_data)(ptk_allocator_t *alloc, cpf_packet_t *self, size_t index, unconnected_data_item_t **element);
} cpf_packet_vtable_t;
```

### Usage Examples
```c
// Simple array access
u8 coil_byte;
msg->vtable->get_coil_data_element(msg, 5, &coil_byte);
msg->vtable->set_coil_data_element(msg, 5, 0xFF);

// Aggregate array access
register_info_t *reg_info;
msg->vtable->get_registers_element(msg, 0, &reg_info);
// reg_info points to the actual element in the array

// Variant array access
cpf_items_element_t item;
packet->vtable->get_items_element(packet, 0, &item);
if (item.type == CPF_ITEMS_CONNECTED_DATA_ITEM) {
    connected_data_item_t *data_item = item.element.connected_data_item;
    // Use data_item...
}

// Type-specific convenience accessor
connected_data_item_t *data_item;
ptk_err err = packet->vtable->get_items_as_connected_data(packet, 0, &data_item);
if (err == PTK_OK) {
    // Successfully got connected data item
} else if (err == PTK_ERR_TYPE_MISMATCH) {
    // Element at index 0 is not a connected_data_item
}
```

## Message Processing Workflow

### Receiving Messages (Field-Indexed Decoding)

```c
// 1. Allocate byte array for received data
ptk_allocator_t *alloc = &PTK_SYSTEM_ALLOCATOR;
u8_array_t *receive_buffer;
u8_array_init(&receive_buffer);
u8_array_resize(&receive_buffer, 4096);  // Max expected message size

// 2. Wrap in ptk_buf_t for stream processing
ptk_buf_t network_buf;
ptk_buf_create(&network_buf, receive_buffer);

// 3. Receive data from socket (from ptk_socket.h)
size_t bytes_received;
ptk_err err = ptk_socket_receive(socket, &network_buf, &bytes_received);
if (err != PTK_OK) return err;

// 4. Create root message instance
eip_message_t *root_message;
err = eip_message_create(alloc, &root_message);
if (err != PTK_OK) return err;

// 5. Decode - copies data into encoded_data and calculates field offsets
err = root_message->vtable->decode(alloc, root_message, &network_buf);
if (err != PTK_OK) return err;

// 6. Access fields through type-safe getters (no pointers to child structures)
u16 command;
err = root_message->vtable->get_header_command(alloc, root_message, &command);

u32 session_handle;
err = root_message->vtable->get_header_session_handle(alloc, root_message, &session_handle);

// Variable-length field access
u8_array_t *cip_data;
err = root_message->vtable->get_cip_payload_data(alloc, root_message, &cip_data);

// 7. Move unused data down for next message
ptk_buf_move_to(&network_buf, 0);

// 8. Process the message data...
// Note: All data remains in encoded format in message->encoded_data
```

### Sending Messages (Hierarchical Encoding)

```c
// 1. Allocate leaf message to send
cip_request_t *cip_request;
ptk_err err = cip_request_create(alloc, &cip_request);
if (err != PTK_OK) return err;

// 2. Set the fields
cip_request->vtable->set_service(alloc, cip_request, 0x4C);  // Read Tag
// ... set other fields ...

// 3. Encode up the hierarchy - each level creates its own buffer
// The context parameter helps resolve ambiguous parent relationships
encode_context_t context = {
    .target_connection_type = CPF_CONNECTED_DATA,
    .session_handle = current_session
};

// This calls parent encoders up the chain:
// cip_request -> cpf_connected_data -> cpf_packet -> eip_message
u8_array_t *encoded_message;
err = cip_request->vtable->encode_to_parent(alloc, cip_request, &context, &encoded_message);

// 4. Result is array of byte arrays from each hierarchy level
// 5. Send via socket
ptk_buf_t send_buf;
ptk_buf_create(&send_buf, encoded_message);
ptk_socket_send(socket, &send_buf);
```

### Handling Ambiguous Parent Relationships

For messages that can appear in multiple parent contexts (like CIP payloads in both connected and unconnected CPF data):

```c
// User-provided context resolution function
ptk_err resolve_cpf_parent_type(const cip_request_t *cip_msg, const void *context,
                               cpf_parent_type_t *parent_type) {
    const encode_context_t *enc_ctx = (const encode_context_t *)context;

    // Use context to determine which parent type to use
    switch (enc_ctx->target_connection_type) {
        case CPF_CONNECTED_DATA:
            *parent_type = CPF_PARENT_CONNECTED_DATA;
            break;
        case CPF_UNCONNECTED_DATA:
            *parent_type = CPF_PARENT_UNCONNECTED_DATA;
            break;
        default:
            return PTK_ERR_INVALID_PARAM;
    }
    return PTK_OK;
}

// Generated encode function uses the resolver
ptk_err cip_request_encode_to_parent(ptk_allocator_t *alloc, const cip_request_t *self,
                                    const void *context, u8_array_t **result) {
    cpf_parent_type_t parent_type;
    ptk_err err = resolve_cpf_parent_type(self, context, &parent_type);
    if (err != PTK_OK) return err;

    switch (parent_type) {
        case CPF_PARENT_CONNECTED_DATA:
            return encode_via_connected_data_parent(alloc, self, context, result);
        case CPF_PARENT_UNCONNECTED_DATA:
            return encode_via_unconnected_data_parent(alloc, self, context, result);
        default:
            return PTK_ERR_INVALID_PARAM;
    }
}
```

### Memory Management Strategy

Each hierarchy level allocates its own byte array using the provided allocator:

```c
// Example: CPF packet encoding
ptk_err cpf_packet_encode(ptk_allocator_t *alloc, ptk_buf_t *dst, const cpf_packet_t *self) {
    // Allocate our own encoding buffer
    u8_array_t *local_buffer;
    u8_array_init(&local_buffer);

    // Encode our fields
    ptk_buf_t local_buf;
    ptk_buf_create(&local_buf, local_buffer);

    // Encode header fields
    ptk_codec_produce_u32(&local_buf, self->interface_handle, PTK_CODEC_LITTLE_ENDIAN);
    ptk_codec_produce_u16(&local_buf, self->timeout, PTK_CODEC_LITTLE_ENDIAN);
    ptk_codec_produce_u16(&local_buf, self->item_count, PTK_CODEC_LITTLE_ENDIAN);

    // Encode each item (calls child encoders)
    for (size_t i = 0; i < self->item_count; i++) {
        cpf_items_element_t item;
        self->vtable->get_items_element(alloc, self, i, &item);

        // Each item type encodes itself
        switch (item.type) {
            case CPF_ITEMS_CONNECTED_DATA_ITEM:
                item.element.connected_data_item->vtable->encode(alloc, &local_buf,
                                                               item.element.connected_data_item);
                break;
            // ... other item types ...
        }
    }

    // Append our buffer to the destination
    ptk_buf_append_array(dst, local_buffer);
    return PTK_OK;
}
```

This approach provides:
- **Clean separation**: Each level manages its own memory
- **Context-driven encoding**: Resolves ambiguous parent relationships
- **Hierarchical processing**: Natural encode/decode chains
- **Flexible allocation**: Custom allocators for different memory strategies

The C file would contain generated C code implementing these patterns.

Fields can be specifically derived as well:

```pdl
def bit_msg = {
    type: message,
    fields: [
        bit_holder: { type: u8, visibility: hidden },
        bit_one: { type: bool, representation: derived, decode: (bit_holder & 0x01), encode: (bit_one ? (bit_holder | 0x01) : (bit_holder & ~0x01)) },
        bit_two: { type: bool, representation: derived, decode: (bit_holder & 0x02), encode: (bit_two ? (bit_holder | 0x02) : (bit_holder & ~0x02)) },
        bit_three: { type: bool, representation: derived, decode: (bit_holder & 0x04), encode: (bit_three ? (bit_holder | 0x04) : (bit_holder & ~0x04)) }
    ]
}
```

## Expressions

Expression are simple constructs of field names (virtual or concrete) and operators:
* '*' - multiplication
* '/' - division
* '+' - addition
* '-' - subtraction
* '%' - modulus
* '<<' - left shift
* '>>' - right shift
* '&' - bitwise AND
* '|' - bitwise OR
* '^' - bitwise XOR
* '~' - bitwise NOT
* '!' - logical NOT
* '&&' - logical AND
* '||' - logical OR
* '?' - ternary operator

Expressions are parsed in operator precendence order.  Parentheses can be used to group operations: `(field1 + field2) * field3` to ensure calculation order.

## Constant definitions

An example:

```pdl
def EIP_SEND_RR_DATA = { type: u16_le, const: 0x006F}
```

These get generated as constants in the .h file.  They are a type:

```pdl
command: EIP_SEND_RR_DATA,
```

## Definitions

Definitions are the only construct at the top of the file level.  They start with "def" followed by a name an "=" and then a type definition.


### Definition field types

There are several fields that can be used in type and field definitions:
- byte_order - an array of byte offsets in the value from the underlying byte data. Little endian u16 is [0, 1].  Big endian u32 is [3, 2, 1, 0].
- check_post_decode - a function or expression to run after decoding the field.
- check_pre_encode - a function or expression to run before encoding the field.
- const - a constant value.
- decode - an expression or a function name. Expressions in parentheses. Function names just bare word.
- encode - as for decode.
- fields - an array of fields for an aggregate type.
- length - for complex array sizing, contains decode and encode expressions/functions to calculate array length.
- match - an expression or function name. Generate a PTK_OK response if it evaluates to true.
- representation - derived or concrete.  Default, concrete.
- type - a type name.  If message, then an aggregate with fields. Can be an array of types in which case the decoder tries them one at a time in order and picks the first one that matches. The encoder is called on the struct's vtable function. For arrays, use `type[]` with length attribute for complex sizing.
- validate - an expression or function name for field validation. Must evaluate to true for field to be valid.
- visibility - hidden or visible.  Default visible.

## Array Syntax

Arrays can be defined in several ways depending on complexity:

### Simple Array Shortcuts

```pdl
def simple_arrays = {
    type: message,
    fields: [
        length: u16_le,
        data: u8[length],                    // Direct field reference shortcut
        fixed_data: u8[256],                 // Constant size shortcut
        item_count: u16_le,
        headers: message_header[item_count]  // Direct field reference shortcut
    ]
}
```

### Complex Array Sizing with Length Attribute

For anything more complex than a direct field reference or constant, use the `length:` attribute:

```pdl
def complex_arrays = {
    type: message,
    fields: [
        total_length: u16_le,
        header_size: u8,

        // Calculated size using expression
        payload: {
            type: u8[],
            length: {
                decode: (total_length - header_size - sizeof(u16_le)),
                encode: (payload.count)  // Use actual array length
            }
        },

        // Function-based size calculation
        compressed_data: {
            type: u8[],
            length: {
                decode: calculate_decompressed_size,
                encode: calculate_compressed_size
            }
        },

        // Namespaced function with validation
        validated_payload: {
            type: u8[],
            length: {
                decode: protocol_utils.decode_payload_length,
                encode: protocol_utils.encode_payload_length
            },
            validate: protocol_utils.validate_payload_integrity
        }
    ]
}
```

## Field References

Field references use the `$` syntax to refer to the parent message context:
- `$` represents the immediate parent message
- `$.field_name` accesses a field in the parent message
- `$.parent.field_name` accesses nested fields in the parent hierarchy
- Root messages cannot use `$` syntax as they have no parent

```pdl
def nested_msg = {
    type: message,
    match: ($.header.command == SOME_COMMAND),  // Reference parent's header.command
    fields: [
        sub_length: u16_le,
        sub_data: {
            type: u8[],
            length: {
                decode: ($.total_length - sub_length),  // Reference parent's total_length
                encode: (sub_data.count)
            }
        }
    ]
}
```

## Validation Examples

```pdl
def validated_msg = {
    type: message,
    fields: [
        command: { type: u16_le, validate: (command >= 0x0001 && command <= 0x00FF) },
        length: { type: u16_le, validate: validate_length_range },  // Function validation
        data: u8[length],
        checksum: { type: u16_le, validate: (checksum == eip_common.calculate_checksum(data)) }  // Namespaced function
    ]
}
```

## Namespace Usage

Imported definitions are accessed using the namespace prefix followed by a dot and the identifier:

```pdl
import protocol_utils = "utils/protocol_helpers.pdl"
import common_types = "common/base_types.pdl"

def complex_message = {
    type: message,
    fields: [
        header: common_types.standard_header,              // Imported type
        payload_size: u32_le,
        payload: {                                         // Complex array sizing
            type: u8[],
            length: {
                decode: protocol_utils.calculate_decode_size,
                encode: protocol_utils.calculate_encode_size
            }
        },
        footer: {
            type: common_types.footer_type,                // Imported type in nested definition
            validate: protocol_utils.validate_footer       // Imported validation function
        }
    ]
}
```
