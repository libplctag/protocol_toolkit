# PTK - Protocol Toolkit

This toolkit aims to make building servers and clients for industrial network protocols easier and safer.
* safe by design. Use only safe constructs and functions.
* cross platform.  Develop on any of the major OSes.  Deploy on major OSes and embedded systems with a few MB of memory.
* clarity and correctness over performance.  Your performance does not matter when your embedded system crashed.
* composable components.  All the pieces should work together.
* a comprehensive but not overwhelming set of components.
* use resources wisely.  Low memory overhead.  Low memory fragmentation as many embedded systems have poor malloc implementations.
* C11 for portability.


## Network Functions

TCP and UDP sockets are fully wrapped and use slice arrays with arena allocators for efficient data handling.

## Composable Functions and Errors

To make functions more composable and easier to use, functions in the toolkit return pointers or data directly.  Status is stored within
the thread local data.  If the returned data is a value that is known to be bad then the error code can be retrieved.  Or it can just be retrieved
all the time.  


## Memory Management - Arena/Scratch Allocators

```c
/* Arena/Scratch Allocator API */

/* Create arena with initial capacity */
ptk_scratch_t* ptk_scratch_create(size_t initial_capacity);

/* Reset arena to beginning (fast, no deallocation) */
void ptk_scratch_reset(ptk_scratch_t* scratch);

/* Low-level allocation - returns raw byte slice */
ptk_slice_bytes_t ptk_scratch_alloc(ptk_scratch_t* scratch, size_t size);
ptk_slice_bytes_t ptk_scratch_alloc_aligned(ptk_scratch_t* scratch, size_t size, size_t alignment);

/* Type-safe allocation - see slice section for details */
#define ptk_scratch_alloc_slice(scratch, slice_name, count) \
    ptk_slice_##slice_name##_make( \
        (void*)ptk_scratch_alloc_aligned(scratch, \
            ptk_type_info_##slice_name.size * (count), \
            ptk_type_info_##slice_name.alignment).data, \
        (count) \
    )

/* Get current usage stats */
size_t ptk_scratch_used(ptk_scratch_t* scratch);
size_t ptk_scratch_capacity(ptk_scratch_t* scratch);
size_t ptk_scratch_remaining(ptk_scratch_t* scratch);

/* Save/restore position for nested allocations */
ptk_scratch_mark_t ptk_scratch_mark(ptk_scratch_t* scratch);
void ptk_scratch_restore(ptk_scratch_t* scratch, ptk_scratch_mark_t mark);

/* Destroy arena */
void ptk_scratch_destroy(ptk_scratch_t* scratch);

```

## Unified Connection API - No Hidden Allocations

PTK provides a unified connection abstraction that maps cleanly to select() while hiding platform complexity. Everything is treated as a connection with states, and all allocations are explicit.

### Core Connection API - No Hidden Allocations

```c
/* Transport connections - all are event sources directly */
typedef struct {
    ptk_event_source_t base;      /* Must be first - enables polymorphism */
    int fd;
    struct sockaddr_in addr;
    uint32_t connect_timeout_ms;
} ptk_tcp_connection_t;

typedef struct {
    ptk_event_source_t base;      /* Must be first - enables polymorphism */
    int fd;
    struct sockaddr_in addr;
    uint32_t bind_timeout_ms;
} ptk_udp_connection_t;

typedef struct {
    ptk_event_source_t base;      /* Must be first - enables polymorphism */
    int fd;
    char device_path[256];
    int baud_rate;
    uint32_t read_timeout_ms;
} ptk_serial_connection_t;

/* Protocol connections - much larger, contain transport reference + protocol state */
typedef struct {
    ptk_event_source_t base;      /* Must be first - enables polymorphism */
    ptk_tcp_connection_t* transport;
    uint32_t session_handle;      /* Persistent session state */
    uint16_t connection_id;       /* Persistent connection state */
    uint8_t sequence_number;      /* Persistent sequence tracking */
    uint32_t registration_timeout_ms;
    uint8_t sender_context[8];    /* EIP sender context */
    uint16_t encap_sequence;      /* Encapsulation sequence number */
} ptk_eip_connection_t;

typedef struct {
    ptk_event_source_t base;      /* Must be first - enables polymorphism */
    ptk_tcp_connection_t* transport;
    uint16_t transaction_id;      /* Persistent transaction tracking */
    uint8_t unit_id;             /* Persistent unit identifier */
    uint8_t function_code;       /* Last function code used */
    uint16_t retry_count;        /* Protocol retry handling */
    uint32_t response_timeout_ms; /* Protocol-specific timeout */
} ptk_modbus_connection_t;

typedef enum {
    PTK_CONN_DATA_READY = 1,    /* Data available to read */
    PTK_CONN_WRITE_READY = 2,   /* Ready for write */
    PTK_CONN_ERROR = 4,         /* Error condition */
    PTK_CONN_CLOSED = 8,        /* Connection closed */
    PTK_CONN_TIMEOUT = 16       /* Timeout occurred */
} ptk_connection_state_t;

typedef enum {
    PTK_EVENT_SOURCE_TCP = 1,      /* TCP socket */
    PTK_EVENT_SOURCE_UDP = 2,      /* UDP socket */
    PTK_EVENT_SOURCE_SERIAL = 3,   /* Serial port */
    PTK_EVENT_SOURCE_EVENT = 4,    /* Application event */
    PTK_EVENT_SOURCE_TIMER = 5     /* Timer event source */
} ptk_event_source_type_t;

/* Base event source - all connection types include this as first element */
typedef struct {
    ptk_event_source_type_t type;     /* Type of event source */
    ptk_connection_state_t state;     /* Current state */
} ptk_event_source_t;

/* Stack-based connection initialization */
ptk_status_t ptk_init_tcp_connection(ptk_tcp_connection_t* conn, const char* host, uint16_t port);
ptk_status_t ptk_init_udp_connection(ptk_udp_connection_t* conn, const char* host, uint16_t port);
ptk_status_t ptk_init_serial_connection(ptk_serial_connection_t* conn, const char* device, int baud);

/* Protocol-specific connection initialization - takes event source (any connection) */
ptk_status_t ptk_init_eip_connection(ptk_eip_connection_t* eip_conn, ptk_tcp_connection_t* tcp_conn);
ptk_status_t ptk_init_modbus_connection(ptk_modbus_connection_t* mb_conn, ptk_event_source_t* transport_conn);

/* Type-Safe Slice System */

/* Slice declaration macro - creates typed slice with operations */
#define PTK_DECLARE_SLICE_TYPE(slice_name, T) \
    typedef struct { \
        T* data; \
        size_t len; \
    } ptk_slice_##slice_name##_t; \
    \
    static inline ptk_slice_##slice_name##_t ptk_slice_##slice_name##_make(T* data, size_t len) { \
        return (ptk_slice_##slice_name##_t){.data = data, .len = len}; \
    } \
    \
    static inline ptk_slice_##slice_name##_t ptk_slice_##slice_name##_advance(ptk_slice_##slice_name##_t slice, size_t count) { \
        if (count >= slice.len) { \
            return (ptk_slice_##slice_name##_t){.data = slice.data + slice.len, .len = 0}; \
        } \
        return (ptk_slice_##slice_name##_t){.data = slice.data + count, .len = slice.len - count}; \
    } \
    \
    static inline bool ptk_slice_##slice_name##_is_empty(ptk_slice_##slice_name##_t slice) { \
        return slice.len == 0; \
    } \
    \
    static inline ptk_slice_##slice_name##_t ptk_slice_##slice_name##_truncate(ptk_slice_##slice_name##_t slice, size_t len) { \
        if (len >= slice.len) return slice; \
        return (ptk_slice_##slice_name##_t){.data = slice.data, .len = len}; \
    } \
    \
    static const ptk_type_info_t ptk_type_info_##slice_name = { \
        .size = sizeof(T), \
        .alignment = _Alignof(T) \
    }

/* Type information for allocation */
typedef struct {
    size_t size;
    size_t alignment;
} ptk_type_info_t;

/* Common slice types */
PTK_DECLARE_SLICE_TYPE(bytes, uint8_t);      /* ptk_slice_bytes_t */
PTK_DECLARE_SLICE_TYPE(u16, uint16_t);       /* ptk_slice_u16_t */  
PTK_DECLARE_SLICE_TYPE(u32, uint32_t);       /* ptk_slice_u32_t */
PTK_DECLARE_SLICE_TYPE(chars, char);         /* ptk_slice_chars_t */
PTK_DECLARE_SLICE_TYPE(str_ptrs, char*);     /* ptk_slice_str_ptrs_t */

/* Backward compatibility - byte slice is the default */
typedef ptk_slice_bytes_t ptk_slice_t;

/* Byte slice operations (backward compatible) */
static inline ptk_slice_t ptk_slice_make(uint8_t* data, size_t len) {
    return ptk_slice_bytes_make(data, len);
}

static inline bool ptk_slice_is_empty(ptk_slice_t slice) {
    return ptk_slice_bytes_is_empty(slice);
}

static inline ptk_slice_t ptk_slice_advance(ptk_slice_t slice, size_t bytes) {
    return ptk_slice_bytes_advance(slice, bytes);
}

static inline ptk_slice_t ptk_slice_truncate(ptk_slice_t slice, size_t len) {
    return ptk_slice_bytes_truncate(slice, len);
}

/* Slice allocation (backward compatible) */
#define ptk_slice_alloc(scratch, size) ptk_scratch_alloc_slice(scratch, bytes, size)

/* Additional slice manipulation helpers for framing */
ptk_slice_t ptk_slice_from_end(ptk_slice_t slice, size_t offset); /* Slice from end */
ptk_slice_t ptk_slice_consumed(ptk_slice_t original, ptk_slice_t remaining); /* What was consumed */

/* Endianness enumeration for serialization */
typedef enum {
    PTK_ENDIAN_BIG,     /* Network byte order (big endian) */
    PTK_ENDIAN_LITTLE,  /* Little endian */
    PTK_ENDIAN_HOST     /* Host native byte order */
} ptk_endian_t;

/* Reading from byte slices - returns value, updates slice */
uint8_t ptk_read_uint8(ptk_slice_t* slice);
uint16_t ptk_read_uint16(ptk_slice_t* slice, ptk_endian_t endian);
uint32_t ptk_read_uint32(ptk_slice_t* slice, ptk_endian_t endian);
uint64_t ptk_read_uint64(ptk_slice_t* slice, ptk_endian_t endian);
float ptk_read_float32(ptk_slice_t* slice, ptk_endian_t endian);
double ptk_read_float64(ptk_slice_t* slice, ptk_endian_t endian);
size_t ptk_read_bytes(ptk_slice_t* src, ptk_slice_t dest);

/* Convenience macros for common endianness */
#define ptk_read_uint16_be(slice) ptk_read_uint16(slice, PTK_ENDIAN_BIG)
#define ptk_read_uint16_le(slice) ptk_read_uint16(slice, PTK_ENDIAN_LITTLE)
#define ptk_read_uint32_be(slice) ptk_read_uint32(slice, PTK_ENDIAN_BIG)
#define ptk_read_uint32_le(slice) ptk_read_uint32(slice, PTK_ENDIAN_LITTLE)
#define ptk_read_uint64_be(slice) ptk_read_uint64(slice, PTK_ENDIAN_BIG)
#define ptk_read_uint64_le(slice) ptk_read_uint64(slice, PTK_ENDIAN_LITTLE)
#define ptk_read_float32_be(slice) ptk_read_float32(slice, PTK_ENDIAN_BIG)
#define ptk_read_float32_le(slice) ptk_read_float32(slice, PTK_ENDIAN_LITTLE)
#define ptk_read_float64_be(slice) ptk_read_float64(slice, PTK_ENDIAN_BIG)
#define ptk_read_float64_le(slice) ptk_read_float64(slice, PTK_ENDIAN_LITTLE)

/* Writing to byte slices - updates slice, returns status */
ptk_status_t ptk_write_uint8(ptk_slice_t* slice, uint8_t value);
ptk_status_t ptk_write_uint16(ptk_slice_t* slice, uint16_t value, ptk_endian_t endian);
ptk_status_t ptk_write_uint32(ptk_slice_t* slice, uint32_t value, ptk_endian_t endian);
ptk_status_t ptk_write_uint64(ptk_slice_t* slice, uint64_t value, ptk_endian_t endian);
ptk_status_t ptk_write_float32(ptk_slice_t* slice, float value, ptk_endian_t endian);
ptk_status_t ptk_write_float64(ptk_slice_t* slice, double value, ptk_endian_t endian);
ptk_status_t ptk_write_bytes(ptk_slice_t* dest, ptk_slice_t src);

/* Convenience macros for common endianness */
#define ptk_write_uint16_be(slice, value) ptk_write_uint16(slice, value, PTK_ENDIAN_BIG)
#define ptk_write_uint16_le(slice, value) ptk_write_uint16(slice, value, PTK_ENDIAN_LITTLE)
#define ptk_write_uint32_be(slice, value) ptk_write_uint32(slice, value, PTK_ENDIAN_BIG)
#define ptk_write_uint32_le(slice, value) ptk_write_uint32(slice, value, PTK_ENDIAN_LITTLE)
#define ptk_write_uint64_be(slice, value) ptk_write_uint64(slice, value, PTK_ENDIAN_BIG)
#define ptk_write_uint64_le(slice, value) ptk_write_uint64(slice, value, PTK_ENDIAN_LITTLE)
#define ptk_write_float32_be(slice, value) ptk_write_float32(slice, value, PTK_ENDIAN_BIG)
#define ptk_write_float32_le(slice, value) ptk_write_float32(slice, value, PTK_ENDIAN_LITTLE)
#define ptk_write_float64_be(slice, value) ptk_write_float64(slice, value, PTK_ENDIAN_BIG)
#define ptk_write_float64_le(slice, value) ptk_write_float64(slice, value, PTK_ENDIAN_LITTLE)

/* I/O operations with slices - polymorphic, work with any event source */
ptk_slice_t ptk_connection_read(ptk_event_source_t* conn, ptk_slice_t buffer, uint32_t timeout_ms);
ptk_slice_t ptk_connection_write(ptk_event_source_t* conn, ptk_slice_t data, uint32_t timeout_ms);

/* Server operations */
ptk_status_t ptk_init_tcp_server(ptk_tcp_connection_t* server, uint16_t port);
ptk_status_t ptk_server_accept(ptk_tcp_connection_t* server, ptk_tcp_connection_t* client_conn);

/* Event connection for app-level signaling (thread-safe) */
typedef struct {
    ptk_event_source_t base;      /* Must be first - enables polymorphism */
    uint8_t buffer[64];           /* Small message buffer */
    volatile uint16_t write_pos;  /* Write position (atomic) */
    volatile uint16_t read_pos;   /* Read position (atomic) */
    uint32_t capacity;           /* Buffer capacity */
} ptk_event_connection_t;

/* Timer event source (thread-local, not thread-safe) */
typedef struct {
    ptk_event_source_t base;      /* Must be first - enables polymorphism */
    uint32_t interval_ms;         /* Timer interval in milliseconds */
    uint32_t next_fire_time_ms;   /* Next time timer should fire (absolute time) */
    uint32_t timer_id;            /* User-specified timer identifier */
    bool repeating;               /* Whether timer repeats or is one-shot */
    bool active;                  /* Whether timer is currently active */
} ptk_timer_event_source_t;

/* Event connection operations (thread-safe) */
ptk_status_t ptk_init_event_connection(ptk_event_connection_t* conn);
ptk_status_t ptk_event_write(ptk_event_connection_t* conn, const void* data, size_t len);
ptk_status_t ptk_event_read(ptk_event_connection_t* conn, void* data, size_t len, size_t* bytes_read);
bool ptk_event_has_data(ptk_event_connection_t* conn);
void ptk_event_clear(ptk_event_connection_t* conn);

/* Timer operations (thread-local) */
ptk_status_t ptk_init_timer(ptk_timer_event_source_t* timer, 
                           uint32_t interval_ms, 
                           uint32_t timer_id, 
                           bool repeating);
ptk_status_t ptk_timer_start(ptk_timer_event_source_t* timer);
ptk_status_t ptk_timer_stop(ptk_timer_event_source_t* timer);
ptk_status_t ptk_timer_reset(ptk_timer_event_source_t* timer);
bool ptk_timer_has_expired(ptk_timer_event_source_t* timer);
uint32_t ptk_timer_get_id(ptk_timer_event_source_t* timer);

/* Platform-optimized time functions */
uint32_t ptk_get_current_time_ms(void);  /* Platform-specific implementation */

/* Thread support - minimal RTOS abstraction */
typedef ptk_pal_thread_t ptk_thread_t;
typedef ptk_atomic_t ptk_atomic_t;

/* Thread operations - builds on PAL */
ptk_status_t ptk_thread_create(ptk_thread_t* thread, 
                              ptk_thread_func_t func, 
                              void* arg,
                              size_t stack_size,
                              int priority);
ptk_status_t ptk_thread_join(ptk_thread_t* thread);
void ptk_thread_yield(void);
void ptk_thread_sleep_ms(uint32_t ms);

/* Atomic types - different sizes for various use cases */
typedef volatile uint8_t ptk_atomic8_t;    /* 8-bit atomic (flags, small counters) */
typedef volatile uint16_t ptk_atomic16_t;  /* 16-bit atomic (medium counters) */
typedef volatile uint32_t ptk_atomic32_t;  /* 32-bit atomic (large counters, pointers on 32-bit) */
typedef volatile uint64_t ptk_atomic64_t;  /* 64-bit atomic (large counters, pointers on 64-bit) */

/* Default atomic type for backward compatibility */
typedef ptk_atomic32_t ptk_atomic_t;

/* 8-bit atomic operations */
uint8_t ptk_atomic8_load(ptk_atomic8_t* atomic);
void ptk_atomic8_store(ptk_atomic8_t* atomic, uint8_t value);
uint8_t ptk_atomic8_exchange(ptk_atomic8_t* atomic, uint8_t value);
bool ptk_atomic8_compare_exchange(ptk_atomic8_t* atomic, uint8_t expected, uint8_t desired);

/* 16-bit atomic operations */
uint16_t ptk_atomic16_load(ptk_atomic16_t* atomic);
void ptk_atomic16_store(ptk_atomic16_t* atomic, uint16_t value);
uint16_t ptk_atomic16_exchange(ptk_atomic16_t* atomic, uint16_t value);
bool ptk_atomic16_compare_exchange(ptk_atomic16_t* atomic, uint16_t expected, uint16_t desired);

/* 32-bit atomic operations */
uint32_t ptk_atomic32_load(ptk_atomic32_t* atomic);
void ptk_atomic32_store(ptk_atomic32_t* atomic, uint32_t value);
uint32_t ptk_atomic32_exchange(ptk_atomic32_t* atomic, uint32_t value);
bool ptk_atomic32_compare_exchange(ptk_atomic32_t* atomic, uint32_t expected, uint32_t desired);

/* 64-bit atomic operations */
uint64_t ptk_atomic64_load(ptk_atomic64_t* atomic);
void ptk_atomic64_store(ptk_atomic64_t* atomic, uint64_t value);
uint64_t ptk_atomic64_exchange(ptk_atomic64_t* atomic, uint64_t value);
bool ptk_atomic64_compare_exchange(ptk_atomic64_t* atomic, uint64_t expected, uint64_t desired);

/* Backward compatible 32-bit operations */
static inline uint32_t ptk_atomic_load(ptk_atomic_t* atomic) {
    return ptk_atomic32_load(atomic);
}

static inline void ptk_atomic_store(ptk_atomic_t* atomic, uint32_t value) {
    ptk_atomic32_store(atomic, value);
}

static inline uint32_t ptk_atomic_exchange(ptk_atomic_t* atomic, uint32_t value) {
    return ptk_atomic32_exchange(atomic, value);
}

static inline bool ptk_atomic_compare_exchange(ptk_atomic_t* atomic, uint32_t expected, uint32_t desired) {
    return ptk_atomic32_compare_exchange(atomic, expected, desired);
}

/* Synchronization through event connections - no separate primitives needed */
/* Use existing ptk_event_connection_t for all thread communication:
 * - Semaphore behavior: ptk_event_write() to signal, check state in wait loop
 * - Message passing: ptk_event_write()/ptk_event_read() for data transfer  
 * - Thread coordination: Shutdown signals, status updates, etc.
 * - Unified with main event loop: All sync integrates with ptk_wait_for_multiple()
 */

/* Resource limits - configurable at compile time */
#ifndef PTK_MAX_EVENT_SOURCES
#define PTK_MAX_EVENT_SOURCES 16  /* Default: sockets + timers + app events */
#endif

#ifndef PTK_DEFAULT_THREAD_STACK_SIZE
#define PTK_DEFAULT_THREAD_STACK_SIZE 4096  /* 4KB default stack */
#endif

/* Universal wait function - polymorphic, works with any event source including timers */
int ptk_wait_for_multiple(ptk_event_source_t** event_sources, 
                         size_t count,
                         uint32_t timeout_ms);
```

### Type-Safe Slice Examples

```c
/* Example: Protocol-specific data structures */
typedef struct {
    uint16_t command;
    uint16_t length;
    uint32_t session_handle;
    uint32_t status;
} eip_header_t;

typedef struct {
    uint16_t register_addr;
    uint16_t value;
    bool writable;
} modbus_register_t;

/* Declare typed slices */
PTK_DECLARE_SLICE_TYPE(eip_headers, eip_header_t);
PTK_DECLARE_SLICE_TYPE(modbus_regs, modbus_register_t);
PTK_DECLARE_SLICE_TYPE(conn_ptrs, ptk_tcp_connection_t*);

/* Type-safe allocation and usage */
void example_typed_slices(ptk_scratch_t* scratch) {
    /* Allocate arrays of structures */
    ptk_slice_eip_headers_t headers = ptk_scratch_alloc_slice(scratch, eip_headers, 10);
    ptk_slice_modbus_regs_t registers = ptk_scratch_alloc_slice(scratch, modbus_regs, 100);
    ptk_slice_conn_ptrs_t connections = ptk_scratch_alloc_slice(scratch, conn_ptrs, 8);
    
    /* Type-safe access - no casts needed */
    for (size_t i = 0; i < headers.len; i++) {
        headers.data[i].command = 0x006F;       /* SendRRData */
        headers.data[i].session_handle = 0x12345678;
    }
    
    /* Initialize connection pointer array */
    for (size_t i = 0; i < connections.len; i++) {
        connections.data[i] = &tcp_clients[i];  /* Type-safe pointer assignment */
    }
    
    /* Process registers with type safety */
    for (size_t i = 0; i < registers.len; i++) {
        if (registers.data[i].writable) {
            registers.data[i].value = read_plc_register(registers.data[i].register_addr);
        }
    }
}

/* Parse structured data type-safely */
ptk_slice_u16_t parse_modbus_registers(ptk_slice_t raw_data, ptk_scratch_t* scratch) {
    ptk_slice_u16_t registers = ptk_scratch_alloc_slice(scratch, u16, raw_data.len / 2);
    
    ptk_slice_t parser = raw_data;
    for (size_t i = 0; i < registers.len; i++) {
        registers.data[i] = ptk_read_uint16_be(&parser);  /* Type-safe result */
    }
    
    return registers;
}

/* Example: Comprehensive serialization with different data types and endianness */
void example_comprehensive_serialization(ptk_scratch_t* scratch) {
    ptk_slice_t buffer = ptk_slice_alloc(scratch, 1024);
    ptk_slice_t writer = buffer;
    
    /* EtherNet/IP header (big endian network format) */
    ptk_write_uint16_be(&writer, 0x006F);        /* Command */
    ptk_write_uint16_be(&writer, 0x0020);        /* Length */
    ptk_write_uint32_be(&writer, 0x12345678);    /* Session handle */
    ptk_write_uint32_be(&writer, 0x00000000);    /* Status */
    ptk_write_uint64_be(&writer, 0x0123456789ABCDEF); /* Sender context */
    ptk_write_uint32_be(&writer, 0x00000000);    /* Options */
    
    /* Modbus data (little endian for some PLCs) */
    ptk_write_uint16_le(&writer, 0x0001);        /* Transaction ID */
    ptk_write_uint16_le(&writer, 0x0000);        /* Protocol ID */
    ptk_write_uint16_le(&writer, 0x0006);        /* Length */
    ptk_write_uint8(&writer, 0x01);              /* Unit ID */
    ptk_write_uint8(&writer, 0x03);              /* Function code */
    ptk_write_uint16_le(&writer, 0x0000);        /* Starting address */
    ptk_write_uint16_le(&writer, 0x000A);        /* Quantity */
    
    /* OPC UA data with mixed types */
    ptk_write_uint32_le(&writer, 0x12345678);    /* Node ID (little endian) */
    ptk_write_float32_le(&writer, 123.456f);     /* Float value */
    ptk_write_float64_be(&writer, 987.654321);   /* Double value (big endian) */
    
    /* Host native format for local processing */
    ptk_write_uint64(&writer, 0xFEDCBA9876543210, PTK_ENDIAN_HOST);
    ptk_write_float64(&writer, 3.14159265359, PTK_ENDIAN_HOST);
    
    printf("Serialized %zu bytes\n", buffer.len - writer.len);
    
    /* Parse it back */
    ptk_slice_t reader = buffer;
    reader.len = buffer.len - writer.len;  /* Only read what we wrote */
    
    /* EtherNet/IP header */
    uint16_t eip_command = ptk_read_uint16_be(&reader);
    uint16_t eip_length = ptk_read_uint16_be(&reader);
    uint32_t session = ptk_read_uint32_be(&reader);
    uint32_t status = ptk_read_uint32_be(&reader);
    uint64_t context = ptk_read_uint64_be(&reader);
    uint32_t options = ptk_read_uint32_be(&reader);
    
    printf("EIP: cmd=0x%04X, len=%u, session=0x%08X\n", eip_command, eip_length, session);
    
    /* Modbus data */
    uint16_t mb_trans_id = ptk_read_uint16_le(&reader);
    uint16_t mb_proto_id = ptk_read_uint16_le(&reader);
    uint16_t mb_length = ptk_read_uint16_le(&reader);
    uint8_t unit_id = ptk_read_uint8(&reader);
    uint8_t func_code = ptk_read_uint8(&reader);
    uint16_t start_addr = ptk_read_uint16_le(&reader);
    uint16_t quantity = ptk_read_uint16_le(&reader);
    
    printf("Modbus: trans=%u, func=%u, addr=%u, qty=%u\n", 
           mb_trans_id, func_code, start_addr, quantity);
    
    /* OPC UA data */
    uint32_t node_id = ptk_read_uint32_le(&reader);
    float float_val = ptk_read_float32_le(&reader);
    double double_val = ptk_read_float64_be(&reader);
    
    printf("OPC UA: node=0x%08X, float=%.3f, double=%.6f\n", 
           node_id, float_val, double_val);
    
    /* Host native */
    uint64_t host_uint64 = ptk_read_uint64(&reader, PTK_ENDIAN_HOST);
    double host_double = ptk_read_float64(&reader, PTK_ENDIAN_HOST);
    
    printf("Host native: uint64=0x%016lX, double=%.11f\n", host_uint64, host_double);
}

/* Example: Timer event sources for industrial protocols */
void example_timer_usage(ptk_scratch_t* scratch) {
    /* Stack-allocated timers - no malloc */
    ptk_timer_event_source_t keepalive_timer;
    ptk_timer_event_source_t watchdog_timer;
    ptk_timer_event_source_t poll_timer;
    ptk_tcp_connection_t plc_connection;
    
    /* Initialize timers with different intervals and behaviors */
    ptk_init_timer(&keepalive_timer, 5000, 1, true);  /* 5s repeating keepalive */
    ptk_init_timer(&watchdog_timer, 10000, 2, false); /* 10s one-shot watchdog */
    ptk_init_timer(&poll_timer, 100, 3, true);        /* 100ms repeating poll */
    
    /* Initialize PLC connection */
    ptk_init_tcp_connection(&plc_connection, "192.168.1.100", 502);
    
    /* Start all timers */
    ptk_timer_start(&keepalive_timer);
    ptk_timer_start(&watchdog_timer);
    ptk_timer_start(&poll_timer);
    
    /* Create event source array with mixed types */
    ptk_event_source_t* event_sources[4] = {
        (ptk_event_source_t*)&plc_connection,  /* Socket */
        (ptk_event_source_t*)&keepalive_timer, /* Timer */
        (ptk_event_source_t*)&watchdog_timer,  /* Timer */
        (ptk_event_source_t*)&poll_timer       /* Timer */
    };
    
    while (true) {
        /* Wait for any event - timers integrated seamlessly */
        int ready = ptk_wait_for_multiple(event_sources, 4, 30000); /* 30s max wait */
        
        if (ready > 0) {
            /* Check socket for data */
            if (plc_connection.base.state & PTK_CONN_DATA_READY) {
                ptk_slice_t buffer = ptk_slice_alloc(scratch, 512);
                ptk_slice_t received = ptk_connection_read((ptk_event_source_t*)&plc_connection, buffer, 100);
                if (!ptk_slice_is_empty(received)) {
                    printf("Received %zu bytes from PLC\n", received.len);
                    ptk_timer_reset(&watchdog_timer); /* Reset watchdog on activity */
                }
            }
            
            /* Check timers */
            if (keepalive_timer.base.state & PTK_CONN_DATA_READY) {
                printf("Keepalive timer expired - sending heartbeat\n");
                send_keepalive_message(&plc_connection, scratch);
                /* Timer automatically resets for next interval (repeating=true) */
            }
            
            if (watchdog_timer.base.state & PTK_CONN_DATA_READY) {
                printf("Watchdog timeout - PLC not responding!\n");
                handle_communication_failure(&plc_connection);
                /* One-shot timer - must manually restart if needed */
                ptk_timer_reset(&watchdog_timer);
                ptk_timer_start(&watchdog_timer);
            }
            
            if (poll_timer.base.state & PTK_CONN_DATA_READY) {
                /* Fast polling timer for time-critical operations */
                poll_plc_status(&plc_connection, scratch);
            }
        }
        
        /* Reset scratch for next iteration */
        ptk_scratch_reset(scratch);
    }
}

/* Event-based synchronization examples - no separate sync primitives needed */

/* Semaphore replacement using event connections */
typedef struct {
    ptk_event_connection_t event;
} ptk_binary_semaphore_t;

ptk_status_t ptk_binary_semaphore_init(ptk_binary_semaphore_t* sem) {
    return ptk_init_event_connection(&sem->event);
}

ptk_status_t ptk_binary_semaphore_signal(ptk_binary_semaphore_t* sem) {
    uint8_t signal = 1;
    return ptk_event_write(&sem->event, &signal, 1);
}

bool ptk_binary_semaphore_is_signaled(ptk_binary_semaphore_t* sem) {
    return ptk_event_has_data(&sem->event);
}

void ptk_binary_semaphore_clear(ptk_binary_semaphore_t* sem) {
    ptk_event_clear(&sem->event);
}

/* Multi-threaded PLC system using event-based synchronization */
typedef struct {
    ptk_event_connection_t plc_data_channel;  /* PLC data from reader thread */
    ptk_event_connection_t shutdown_signal;   /* Graceful shutdown coordination */
    ptk_event_connection_t status_updates;    /* Status reports from threads */
    ptk_atomic_t packets_processed;           /* Lock-free statistics counter */
} plc_system_t;

/* PLC data packet structure for inter-thread communication */
typedef struct {
    uint8_t plc_id;          /* Which PLC (0-2) */
    uint8_t data_type;       /* Type of data */
    uint16_t data_length;    /* Length of payload */
    uint8_t data[56];        /* Payload - fits in event connection buffer */
} plc_packet_t;

/* PLC reader thread - runs on dedicated thread for I/O */
void plc_reader_thread(void* arg) {
    plc_system_t* system = (plc_system_t*)arg;
    ptk_scratch_t* scratch = ptk_scratch_create(2048);
    
    /* Multiple PLC connections with timer */
    ptk_tcp_connection_t plc_connections[3];
    ptk_timer_event_source_t poll_timer;
    
    /* Initialize connections */
    ptk_init_tcp_connection(&plc_connections[0], "192.168.1.10", 502);
    ptk_init_tcp_connection(&plc_connections[1], "192.168.1.11", 502);
    ptk_init_tcp_connection(&plc_connections[2], "192.168.1.12", 502);
    ptk_init_timer(&poll_timer, 100, 1, true);  /* 100ms polling */
    ptk_timer_start(&poll_timer);
    
    /* Build event source array */
    ptk_event_source_t* sources[5] = {
        (ptk_event_source_t*)&plc_connections[0],
        (ptk_event_source_t*)&plc_connections[1],
        (ptk_event_source_t*)&plc_connections[2],
        (ptk_event_source_t*)&poll_timer,
        (ptk_event_source_t*)&system->shutdown_signal  /* Check for shutdown */
    };
    
    printf("PLC reader thread started\n");
    
    while (true) {
        int ready = ptk_wait_for_multiple(sources, 5, 1000);
        
        if (ready > 0) {
            /* Check for shutdown signal */
            if (system->shutdown_signal.base.state & PTK_CONN_DATA_READY) {
                printf("PLC reader thread shutting down\n");
                break;
            }
            
            /* Handle PLC data */
            for (int i = 0; i < 3; i++) {
                if (plc_connections[i].base.state & PTK_CONN_DATA_READY) {
                    ptk_slice_t buffer = ptk_slice_alloc(scratch, 256);
                    ptk_slice_t received = ptk_connection_read((ptk_event_source_t*)&plc_connections[i], buffer, 50);
                    
                    if (!ptk_slice_is_empty(received)) {
                        /* Package data for processing thread */
                        plc_packet_t packet = {0};
                        packet.plc_id = i;
                        packet.data_type = 1; /* Raw data */
                        packet.data_length = (received.len < 56) ? received.len : 56;
                        memcpy(packet.data, received.data, packet.data_length);
                        
                        /* Send to processing thread via event channel */
                        ptk_event_write(&system->plc_data_channel, &packet, sizeof(packet));
                        
                        printf("PLC%d: Forwarded %d bytes to processor\n", i, packet.data_length);
                    }
                }
            }
            
            /* Handle polling timer */
            if (poll_timer.base.state & PTK_CONN_DATA_READY) {
                for (int i = 0; i < 3; i++) {
                    send_status_poll(&plc_connections[i], scratch);
                }
            }
        }
        
        ptk_scratch_reset(scratch);
    }
    
    ptk_scratch_destroy(scratch);
}

/* Data processing thread - handles PLC data from reader thread */
void data_processor_thread(void* arg) {
    plc_system_t* system = (plc_system_t*)arg;
    ptk_scratch_t* scratch = ptk_scratch_create(1024);
    
    /* Event sources for this thread */
    ptk_event_source_t* sources[2] = {
        (ptk_event_source_t*)&system->plc_data_channel,
        (ptk_event_source_t*)&system->shutdown_signal
    };
    
    printf("Data processor thread started\n");
    uint32_t processed_count = 0;
    
    while (true) {
        int ready = ptk_wait_for_multiple(sources, 2, 1000);
        
        if (ready > 0) {
            /* Check for shutdown */
            if (system->shutdown_signal.base.state & PTK_CONN_DATA_READY) {
                printf("Data processor thread shutting down\n");
                break;
            }
            
            /* Process incoming PLC data */
            if (system->plc_data_channel.base.state & PTK_CONN_DATA_READY) {
                plc_packet_t packet;
                size_t bytes_read;
                
                while (ptk_event_read(&system->plc_data_channel, &packet, sizeof(packet), &bytes_read) == PTK_OK) {
                    if (bytes_read == sizeof(packet)) {
                        /* Process the packet */
                        process_plc_packet(&packet, scratch);
                        
                        /* Update statistics atomically */
                        processed_count++;
                        ptk_atomic_store(&system->packets_processed, processed_count);
                        
                        if (processed_count % 100 == 0) {
                            printf("Processed %u packets\n", processed_count);
                        }
                    }
                }
            }
        }
        
        ptk_scratch_reset(scratch);
    }
    
    ptk_scratch_destroy(scratch);
}

/* Main application showing event-based multi-threading */
void event_based_multithreaded_app(void) {
    plc_system_t system = {0};
    ptk_thread_t reader_thread;
    ptk_thread_t processor_thread;
    
    /* Initialize event-based synchronization */
    ptk_init_event_connection(&system.plc_data_channel);
    ptk_init_event_connection(&system.shutdown_signal);
    ptk_init_event_connection(&system.status_updates);
    ptk_atomic_store(&system.packets_processed, 0);
    
    /* Create threads - only basic thread creation needed */
    ptk_thread_create(&reader_thread, plc_reader_thread, &system,
                     PTK_DEFAULT_THREAD_STACK_SIZE, 5);    /* High priority for I/O */
    
    ptk_thread_create(&processor_thread, data_processor_thread, &system,
                     PTK_DEFAULT_THREAD_STACK_SIZE, 3);    /* Lower priority for processing */
    
    printf("Event-based multi-threaded PLC system started\n");
    
    /* Main thread can also participate in event loop */
    ptk_timer_event_source_t status_timer;
    ptk_init_timer(&status_timer, 5000, 1, true);  /* 5 second status reports */
    ptk_timer_start(&status_timer);
    
    ptk_event_source_t* main_sources[2] = {
        (ptk_event_source_t*)&status_timer,
        (ptk_event_source_t*)&system.status_updates
    };
    
    /* Run for 30 seconds */
    for (int i = 0; i < 6; i++) {
        int ready = ptk_wait_for_multiple(main_sources, 2, 5000);
        
        if (ready > 0) {
            if (status_timer.base.state & PTK_CONN_DATA_READY) {
                uint32_t processed = ptk_atomic_load(&system.packets_processed);
                printf("System status: %u packets processed\n", processed);
            }
        }
    }
    
    /* Shutdown using event signaling */
    printf("Initiating shutdown...\n");
    uint8_t shutdown_msg = 1;
    ptk_event_write(&system.shutdown_signal, &shutdown_msg, 1);
    
    /* Wait for threads to complete gracefully */
    ptk_thread_join(&reader_thread);
    ptk_thread_join(&processor_thread);
    
    printf("System shutdown complete - %u total packets processed\n",
           ptk_atomic_load(&system.packets_processed));
}

/* Real-world example: Industrial PLC monitoring with multiple timers */
void industrial_plc_monitor(ptk_scratch_t* scratch) {
    /* Multiple PLCs with different timing requirements */
    ptk_tcp_connection_t plc_connections[3];
    ptk_timer_event_source_t keepalive_timers[3];
    ptk_timer_event_source_t status_poll_timer;
    ptk_timer_event_source_t alarm_check_timer;
    ptk_timer_event_source_t report_timer;
    
    /* Initialize PLC connections */
    ptk_init_tcp_connection(&plc_connections[0], "192.168.1.10", 502);  /* Modbus */
    ptk_init_tcp_connection(&plc_connections[1], "192.168.1.11", 44818); /* EtherNet/IP */
    ptk_init_tcp_connection(&plc_connections[2], "192.168.1.12", 502);   /* Modbus */
    
    /* Initialize timers with industrial timing requirements */
    ptk_init_timer(&keepalive_timers[0], 30000, 100, true);  /* 30s keepalive for PLC1 */
    ptk_init_timer(&keepalive_timers[1], 60000, 101, true);  /* 60s keepalive for PLC2 */
    ptk_init_timer(&keepalive_timers[2], 30000, 102, true);  /* 30s keepalive for PLC3 */
    ptk_init_timer(&status_poll_timer, 1000, 200, true);     /* 1s status polling */
    ptk_init_timer(&alarm_check_timer, 500, 300, true);      /* 500ms alarm checking */
    ptk_init_timer(&report_timer, 300000, 400, true);        /* 5min reporting */
    
    /* Start all timers */
    for (int i = 0; i < 3; i++) {
        ptk_timer_start(&keepalive_timers[i]);
    }
    ptk_timer_start(&status_poll_timer);
    ptk_timer_start(&alarm_check_timer);
    ptk_timer_start(&report_timer);
    
    /* Build unified event source array */
    ptk_event_source_t* all_sources[9] = {
        /* PLC connections */
        (ptk_event_source_t*)&plc_connections[0],
        (ptk_event_source_t*)&plc_connections[1], 
        (ptk_event_source_t*)&plc_connections[2],
        /* Keepalive timers */
        (ptk_event_source_t*)&keepalive_timers[0],
        (ptk_event_source_t*)&keepalive_timers[1],
        (ptk_event_source_t*)&keepalive_timers[2],
        /* System timers */
        (ptk_event_source_t*)&status_poll_timer,
        (ptk_event_source_t*)&alarm_check_timer,
        (ptk_event_source_t*)&report_timer
    };
    
    printf("Industrial monitoring started - 3 PLCs, 6 timers\n");
    
    while (true) {
        /* Wait for any event - optimal timeout calculated automatically */
        int ready = ptk_wait_for_multiple(all_sources, 9, 60000); /* Max 1min wait */
        
        if (ready > 0) {
            /* Handle PLC data */
            for (int i = 0; i < 3; i++) {
                if (plc_connections[i].base.state & PTK_CONN_DATA_READY) {
                    printf("PLC%d: Data received\n", i + 1);
                    process_plc_data(&plc_connections[i], i, scratch);
                }
            }
            
            /* Handle keepalive timers */
            for (int i = 0; i < 3; i++) {
                if (keepalive_timers[i].base.state & PTK_CONN_DATA_READY) {
                    printf("PLC%d: Sending keepalive\n", i + 1);
                    send_plc_keepalive(&plc_connections[i], i, scratch);
                }
            }
            
            /* Handle system timers */
            if (status_poll_timer.base.state & PTK_CONN_DATA_READY) {
                poll_all_plc_status(plc_connections, 3, scratch);
            }
            
            if (alarm_check_timer.base.state & PTK_CONN_DATA_READY) {
                check_plc_alarms(plc_connections, 3, scratch);
            }
            
            if (report_timer.base.state & PTK_CONN_DATA_READY) {
                printf("Generating 5-minute status report\n");
                generate_status_report(plc_connections, 3, scratch);
            }
        }
        
        ptk_scratch_reset(scratch);
    }
}
```

### Slice-Based Reading Example

```c
#include "ptk.h"

int main() {
    ptk_tcp_connection_t tcp_conn;
    ptk_eip_connection_t eip_conn;
    ptk_scratch_t* scratch = NULL;
    
    ptk_init_thread_local();
    
    /* Initialize connections */
    ptk_init_tcp_connection(&tcp_conn, "192.168.1.100", 44818);
    ptk_init_eip_connection(&eip_conn, &tcp_conn);
    
    scratch = ptk_scratch_create(4096);
    
    while (true) {
        /* Allocate slice for incoming data */
        ptk_slice_t recv_slice = ptk_slice_alloc(scratch, 2048);
        
        /* Read data into slice */
        ptk_slice_t received = ptk_connection_read((ptk_event_source_t*)&eip_conn, recv_slice, 5000);
        
        if (ptk_slice_is_empty(received)) {
            printf("No data received or timeout\n");
            continue;
        }
        
        printf("Received %zu bytes\n", received.len);
        
        /* Parse EtherNet/IP header */
        ptk_slice_t slice = received;
        uint16_t command = ptk_read_uint16_be(&slice);
        uint16_t length = ptk_read_uint16_be(&slice);
        uint32_t session_handle = ptk_read_uint32_be(&slice);
        uint32_t status = ptk_read_uint32_be(&slice);
        
        printf("EIP: cmd=0x%04X, len=%u, session=0x%08X, status=0x%08X\n",
               command, length, session_handle, status);
        
        /* slice now points to payload data */
        printf("Payload: %zu bytes remaining\n", slice.len);
        
        /* Reset scratch for next iteration */
        ptk_scratch_reset(scratch);
    }
    
    ptk_scratch_destroy(scratch);
    ptk_cleanup_thread_local();
    return 0;
}
```

### Slice-Based Writing Example

```c
#include "ptk.h"

void send_eip_request(ptk_eip_connection_t* conn, ptk_scratch_t* scratch) {
    /* Allocate slice for outgoing data */
    ptk_slice_t original_slice = ptk_slice_alloc(scratch, 1024);
    ptk_slice_t slice = original_slice;
    
    /* Build EtherNet/IP request */
    if (ptk_write_uint16_be(&slice, 0x006F) != PTK_OK ||      /* SendRRData command */
        ptk_write_uint16_be(&slice, 16) != PTK_OK ||          /* Length */
        ptk_write_uint32_be(&slice, 0x12345678) != PTK_OK ||  /* Session handle */
        ptk_write_uint32_be(&slice, 0x00000000) != PTK_OK ||  /* Status */
        ptk_write_uint64_be(&slice, 0x0123456789ABCDEF) != PTK_OK ||  /* Sender context (64-bit) */
        ptk_write_uint32_be(&slice, 0x00000000) != PTK_OK) {  /* Options */
        printf("Failed to write EIP header\n");
        return;
    }
    
    /* Add some payload data */
    uint8_t payload[] = {0x01, 0x02, 0x03, 0x04};
    ptk_slice_t payload_slice = ptk_slice_make(payload, sizeof(payload));
    if (ptk_write_bytes(&slice, payload_slice) != PTK_OK) {
        printf("Failed to write payload\n");
        return;
    }
    
    /* Calculate how much we wrote by comparing original slice with remaining */
    ptk_slice_t written_data = ptk_slice_make(original_slice.data, original_slice.len - slice.len);
    
    printf("Sending %zu bytes\n", written_data.len);
    
    /* Send the data */
    ptk_slice_t unsent = written_data;
    while (!ptk_slice_is_empty(unsent)) {
        ptk_event_source_t* sources[] = { (ptk_event_source_t*)conn->transport };
        int ready = ptk_wait_for_multiple(sources, 1, 1000);
        
        if (ready > 0 && (conn->transport->base.state & PTK_CONN_WRITE_READY)) {
            unsent = ptk_connection_write((ptk_event_source_t*)conn, unsent, 100);
            
            if (!ptk_slice_is_empty(unsent)) {
                printf("Partial send: %zu bytes remaining\n", unsent.len);
            }
        }
    }
    
    printf("Send complete\n");
}

/* Example showing slice-to-slice operations */
void process_data_blocks(ptk_slice_t input_data, ptk_scratch_t* scratch) {
    ptk_slice_t slice = input_data;
    
    while (!ptk_slice_is_empty(slice)) {
        /* Read block header */
        uint16_t block_type = ptk_read_uint16_be(&slice);
        uint16_t block_size = ptk_read_uint16_be(&slice);
        
        if (block_size > slice.len) {
            printf("Invalid block size\n");
            break;
        }
        
        /* Extract block data as a slice */
        ptk_slice_t block_data = ptk_slice_make(slice.data, block_size);
        slice.data += block_size;
        slice.len -= block_size;
        
        /* Process the block */
        switch (block_type) {
            case 0x1001: /* Data block */
                {
                    /* Copy to output buffer */
                    ptk_slice_t output = ptk_slice_alloc(scratch, block_size);
                    ptk_slice_t src = block_data;
                    size_t copied = ptk_read_bytes(&src, output); /* Copy block_data to output */
                    printf("Processed data block: %zu bytes\n", copied);
                }
                break;
                
            case 0x1002: /* Config block */
                {
                    /* Parse config directly from slice */
                    ptk_slice_t config = block_data;
                    uint32_t config_version = ptk_read_uint32_be(&config);
                    uint32_t flags = ptk_read_uint32_be(&config);
                    printf("Config: version=%u, flags=0x%08X\n", config_version, flags);
                }
                break;
                
            default:
                printf("Unknown block type: 0x%04X\n", block_type);
                break;
        }
    }
}
```

### Framing Example with Slice Helpers

```c
#include "ptk.h"

/* Read complete message with framing support */
ptk_slice_t read_complete_message(ptk_eip_connection_t* conn, ptk_scratch_t* scratch) {
    /* Allocate buffer for maximum possible message */
    ptk_slice_t recv_buffer = ptk_slice_alloc(scratch, 4096);
    ptk_slice_t available_space = recv_buffer;
    size_t total_received = 0;
    uint16_t expected_length = 0;
    
    while (true) {
        /* Read more data into remaining space */
        ptk_slice_t newly_received = ptk_connection_read((ptk_event_source_t*)conn, available_space, 1000);
        
        if (ptk_slice_is_empty(newly_received)) {
            /* No data received */
            break;
        }
        
        /* Update total received count */
        total_received += newly_received.len;
        
        /* Get slice of all data received so far */
        ptk_slice_t all_received = ptk_slice_truncate(recv_buffer, total_received);
        
        /* Once we have enough for header, determine expected length */
        if (expected_length == 0 && all_received.len >= 4) {
            ptk_slice_t header = all_received;
            uint16_t command = ptk_read_uint16_be(&header);
            expected_length = ptk_read_uint16_be(&header);
            expected_length += 24; /* Add header size */
            
            printf("Expecting %u byte message\n", expected_length);
        }
        
        /* Check if we have complete message */
        if (expected_length > 0 && all_received.len >= expected_length) {
            /* Return slice of exactly the complete message */
            return ptk_slice_truncate(all_received, expected_length);
        }
        
        /* Calculate remaining space for next read */
        available_space = ptk_slice_from_end(recv_buffer, total_received);
        
        /* Check if we have space for more data */
        if (ptk_slice_is_empty(available_space)) {
            printf("Buffer full before complete message received\n");
            break;
        }
    }
    
    /* Return empty slice if no complete message */
    return ptk_slice_make(NULL, 0);
}

/* Send large message with partial write support */
void send_large_message(ptk_eip_connection_t* conn, ptk_slice_t message) {
    ptk_slice_t unsent = message;
    
    printf("Sending %zu byte message\n", message.len);
    
    while (!ptk_slice_is_empty(unsent)) {
        /* Wait for socket to be ready */
        ptk_event_source_t* sources[] = { (ptk_event_source_t*)conn->transport };
        int ready = ptk_wait_for_multiple(sources, 1, 1000);
        
        if (ready > 0 && (conn->transport->base.state & PTK_CONN_WRITE_READY)) {
            ptk_slice_t previous_unsent = unsent;
            unsent = ptk_connection_write((ptk_event_source_t*)conn, unsent, 100);
            
            /* Calculate how much was sent this iteration */
            ptk_slice_t sent_this_time = ptk_slice_consumed(previous_unsent, unsent);
            
            if (!ptk_slice_is_empty(sent_this_time)) {
                printf("Sent %zu bytes, %zu remaining\n", 
                       sent_this_time.len, unsent.len);
            }
            
            if (!ptk_slice_is_empty(unsent)) {
                printf("Partial send: %zu bytes still to send\n", unsent.len);
            }
        }
    }
    
    printf("Message send complete\n");
}

/* Example usage */
int main() {
    ptk_tcp_connection_t tcp_conn;
    ptk_eip_connection_t eip_conn;
    ptk_scratch_t* scratch = NULL;
    
    ptk_init_thread_local();
    ptk_init_tcp_connection(&tcp_conn, "192.168.1.100", 44818);
    ptk_init_eip_connection(&eip_conn, &tcp_conn);
    scratch = ptk_scratch_create(8192);
    
    while (true) {
        /* Read complete message using framing */
        ptk_slice_t message = read_complete_message(&eip_conn, scratch);
        
        if (!ptk_slice_is_empty(message)) {
            printf("Received complete message: %zu bytes\n", message.len);
            
            /* Process message */
            process_eip_message(message, scratch);
            
            /* Create response */
            ptk_slice_t response = create_eip_response(message, scratch);
            
            /* Send response with automatic partial write handling */
            send_large_message(&eip_conn, response);
        }
        
        /* Reset scratch for next iteration */
        ptk_scratch_reset(scratch);
    }
    
    ptk_scratch_destroy(scratch);
    ptk_cleanup_thread_local();
    return 0;
}
```

### Slice Helper Implementation

```c
/* ptk_slice.c - Slice manipulation helpers */

/* Byte slice helpers (most are now generated by macro, these are the special ones) */
ptk_slice_t ptk_slice_from_end(ptk_slice_t slice, size_t offset) {
    if (offset >= slice.len) {
        return ptk_slice_make(slice.data + slice.len, 0);
    }
    return ptk_slice_make(slice.data + offset, slice.len - offset);
}

ptk_slice_t ptk_slice_consumed(ptk_slice_t original, ptk_slice_t remaining) {
    /* Calculate what was consumed between original and remaining slices */
    if (remaining.data < original.data || 
        remaining.data > original.data + original.len) {
        /* Invalid - remaining not within original */
        return ptk_slice_make(NULL, 0);
    }
    
    size_t consumed_bytes = remaining.data - original.data;
    return ptk_slice_make(original.data, consumed_bytes);
}

/* Example of generating slice helpers for custom types */
void example_custom_slices(void) {
    /* Industrial protocol data types */
    typedef struct {
        uint32_t timestamp;
        uint16_t alarm_code;
        uint8_t severity;
        char message[64];
    } alarm_event_t;
    
    /* Declare slice for alarm events */
    PTK_DECLARE_SLICE_TYPE(alarms, alarm_event_t);
    
    /* Now ptk_slice_alarms_t, ptk_slice_alarms_make(), 
       ptk_slice_alarms_advance(), etc. are all available */
}

/* Serialization function implementations */

/* Host byte order detection */
static inline bool ptk_is_little_endian(void) {
    const uint16_t test = 0x0001;
    return *(const uint8_t*)&test == 0x01;
}

/* 16-bit serialization */
uint16_t ptk_read_uint16(ptk_slice_t* slice, ptk_endian_t endian) {
    if (slice->len < 2) {
        ptk_set_last_error(PTK_ERROR_BUFFER_UNDERFLOW);
        return 0;
    }
    
    uint16_t value;
    switch (endian) {
        case PTK_ENDIAN_BIG:
            value = ((uint16_t)slice->data[0] << 8) | slice->data[1];
            break;
        case PTK_ENDIAN_LITTLE:
            value = slice->data[0] | ((uint16_t)slice->data[1] << 8);
            break;
        case PTK_ENDIAN_HOST:
            value = *(uint16_t*)slice->data;  /* Direct copy for host order */
            break;
    }
    
    slice->data += 2;
    slice->len -= 2;
    return value;
}

ptk_status_t ptk_write_uint16(ptk_slice_t* slice, uint16_t value, ptk_endian_t endian) {
    if (slice->len < 2) {
        return PTK_ERROR_BUFFER_OVERFLOW;
    }
    
    switch (endian) {
        case PTK_ENDIAN_BIG:
            slice->data[0] = (value >> 8) & 0xFF;
            slice->data[1] = value & 0xFF;
            break;
        case PTK_ENDIAN_LITTLE:
            slice->data[0] = value & 0xFF;
            slice->data[1] = (value >> 8) & 0xFF;
            break;
        case PTK_ENDIAN_HOST:
            *(uint16_t*)slice->data = value;  /* Direct copy for host order */
            break;
    }
    
    slice->data += 2;
    slice->len -= 2;
    return PTK_OK;
}

/* 64-bit serialization */
uint64_t ptk_read_uint64(ptk_slice_t* slice, ptk_endian_t endian) {
    if (slice->len < 8) {
        ptk_set_last_error(PTK_ERROR_BUFFER_UNDERFLOW);
        return 0;
    }
    
    uint64_t value;
    switch (endian) {
        case PTK_ENDIAN_BIG:
            value = ((uint64_t)slice->data[0] << 56) |
                    ((uint64_t)slice->data[1] << 48) |
                    ((uint64_t)slice->data[2] << 40) |
                    ((uint64_t)slice->data[3] << 32) |
                    ((uint64_t)slice->data[4] << 24) |
                    ((uint64_t)slice->data[5] << 16) |
                    ((uint64_t)slice->data[6] << 8) |
                    slice->data[7];
            break;
        case PTK_ENDIAN_LITTLE:
            value = slice->data[0] |
                    ((uint64_t)slice->data[1] << 8) |
                    ((uint64_t)slice->data[2] << 16) |
                    ((uint64_t)slice->data[3] << 24) |
                    ((uint64_t)slice->data[4] << 32) |
                    ((uint64_t)slice->data[5] << 40) |
                    ((uint64_t)slice->data[6] << 48) |
                    ((uint64_t)slice->data[7] << 56);
            break;
        case PTK_ENDIAN_HOST:
            value = *(uint64_t*)slice->data;
            break;
    }
    
    slice->data += 8;
    slice->len -= 8;
    return value;
}

/* Float serialization - uses type punning through union */
float ptk_read_float32(ptk_slice_t* slice, ptk_endian_t endian) {
    union { uint32_t u; float f; } converter;
    converter.u = ptk_read_uint32(slice, endian);
    return converter.f;
}

ptk_status_t ptk_write_float32(ptk_slice_t* slice, float value, ptk_endian_t endian) {
    union { uint32_t u; float f; } converter;
    converter.f = value;
    return ptk_write_uint32(slice, converter.u, endian);
}

double ptk_read_float64(ptk_slice_t* slice, ptk_endian_t endian) {
    union { uint64_t u; double d; } converter;
    converter.u = ptk_read_uint64(slice, endian);
    return converter.d;
}

ptk_status_t ptk_write_float64(ptk_slice_t* slice, double value, ptk_endian_t endian) {
    union { uint64_t u; double d; } converter;
    converter.d = value;
    return ptk_write_uint64(slice, converter.u, endian);
}

### Client Example - No Hidden Allocations

```c
#include "ptk.h"

int main() {
    /* Stack allocated - no malloc */
    ptk_tcp_connection_t tcp_conn;
    ptk_eip_connection_t eip_conn;
    ptk_scratch_t* scratch = NULL;
    bool running = true;
    
    ptk_init_thread_local();
    
    /* Initialize connections on stack */
    ptk_status_t status = ptk_init_tcp_connection(&tcp_conn, "192.168.1.100", 44818);
    if (status != PTK_OK) {
        fprintf(stderr, "Failed to initialize TCP connection\n");
        goto cleanup;
    }
    
    status = ptk_init_eip_connection(&eip_conn, &tcp_conn);
    if (status != PTK_OK) {
        fprintf(stderr, "Failed to initialize EtherNet/IP connection\n");
        goto cleanup;
    }
    
    scratch = ptk_scratch_create(4096); /* 4KB is plenty for most industrial protocols */
    if (!scratch) {
        fprintf(stderr, "Failed to create scratch allocator\n");
        goto cleanup;
    }
    
    while (running) {
        /* Create request PDU in scratch */
        ptk_pdu_t* request = ptk_create_pdu(scratch, 64);
        if (!request) {
            fprintf(stderr, "Failed to create request PDU\n");
            break;
        }
        
        /* Build EtherNet/IP request */
        ptk_write_uint16(request, 0x006F); /* Service: Get Attribute Single */
        ptk_write_uint16(request, 0x0001); /* Class ID */
        ptk_write_uint16(request, 0x0001); /* Instance ID */
        ptk_write_uint16(request, 0x0001); /* Attribute ID */
        
        /* Send request with timeout */
        status = ptk_connection_send_pdu(&eip_conn, request, 1000); /* 1s send timeout */
        if (status != PTK_OK) {
            fprintf(stderr, "Failed to send request: %s\n", ptk_status_string(status));
            break;
        }
        
        /* Read response with timeout - internally uses ptk_wait_for_multiple */
        ptk_pdu_t* response = ptk_connection_read_pdu(&eip_conn, scratch, 5000); /* 5s read timeout */
        if (response) {
            /* Parse response */
            uint16_t service_reply;
            uint8_t general_status;
            uint16_t data_value;
            
            ptk_read_uint16(response, &service_reply);
            ptk_read_uint8(response, &general_status);
            if (general_status == 0x00) {
                ptk_read_uint16(response, &data_value);
                printf("Success: attribute value = %u\n", data_value);
            } else {
                printf("Error: general status = 0x%02X\n", general_status);
            }
        } else {
            ptk_status_t error = ptk_get_last_error();
            if (error == PTK_TIMEOUT) {
                printf("Request timeout\n");
            } else {
                fprintf(stderr, "Read error: %s\n", ptk_status_string(error));
                break;
            }
        }
        
        /* Reset scratch - all PDU data becomes invalid, connection state persists */
        ptk_scratch_reset(scratch);
        sleep(1);
    }
    
cleanup:
    if (scratch) ptk_scratch_destroy(scratch);
    ptk_cleanup_thread_local();
    
    return 0;
}
```

### Server Example - Embedded Multi-Client, No Hidden Allocations

```c
#include "ptk.h"

#define MAX_CLIENTS 8  /* Realistic for most embedded systems */

/* Declare slices for connection management */
PTK_DECLARE_SLICE_TYPE(tcp_conns, ptk_tcp_connection_t);
PTK_DECLARE_SLICE_TYPE(eip_conns, ptk_eip_connection_t);
PTK_DECLARE_SLICE_TYPE(event_sources, ptk_event_source_t*);

int main() {
    /* Stack allocated - no malloc */
    ptk_tcp_connection_t server;
    ptk_tcp_connection_t tcp_clients[MAX_CLIENTS];
    ptk_eip_connection_t eip_clients[MAX_CLIENTS];
    ptk_scratch_t* scratch = NULL;
    size_t num_clients = 0;
    bool running = true;
    
    ptk_init_thread_local();
    
    /* Initialize server on stack */
    ptk_status_t status = ptk_init_tcp_server(&server, 44818);
    if (status != PTK_OK) {
        fprintf(stderr, "Failed to initialize server\n");
        return 1;
    }
    
    scratch = ptk_scratch_create(2048); /* 2KB shared scratch - embedded friendly */
    if (!scratch) {
        fprintf(stderr, "Failed to create scratch allocator\n");
        return 1;
    }
    
    memset(tcp_clients, 0, sizeof(tcp_clients));
    memset(eip_clients, 0, sizeof(eip_clients));
    
    printf("Server listening on port 44818 (max %d clients)...\n", MAX_CLIENTS);
    
    while (running) {
        /* Build array of event sources to monitor using type-safe slice */
        ptk_slice_event_sources_t all_sources = ptk_scratch_alloc_slice(scratch, event_sources, MAX_CLIENTS + 1);
        
        all_sources.data[0] = (ptk_event_source_t*)&server;
        for (size_t i = 0; i < num_clients; i++) {
            all_sources.data[i + 1] = (ptk_event_source_t*)&tcp_clients[i];
        }
        all_sources.len = num_clients + 1; /* Set actual length */
        
        /* Wait for activity on any connection */
        int ready = ptk_wait_for_multiple(all_sources.data, all_sources.len, 1000); /* 1s timeout */
        
        if (ready > 0) {
            /* Check server for new connections */
            if (server.state & PTK_CONN_DATA_READY) {
                if (num_clients < MAX_CLIENTS) {
                    status = ptk_server_accept(&server, &tcp_clients[num_clients]);
                    if (status == PTK_OK) {
                        /* Initialize EtherNet/IP protocol on the new TCP connection */
                        status = ptk_init_eip_connection(&eip_clients[num_clients], &tcp_clients[num_clients]);
                        if (status == PTK_OK) {
                            num_clients++;
                            printf("New client connected (%zu total)\n", num_clients);
                        } else {
                            /* Close the TCP connection if protocol init failed */
                            close(tcp_clients[num_clients].fd);
                        }
                    }
                } else {
                    /* Server full - accept and immediately close */
                    ptk_tcp_connection_t temp_conn;
                    if (ptk_server_accept(&server, &temp_conn) == PTK_OK) {
                        printf("Server full, rejecting connection\n");
                        close(temp_conn.fd);
                    }
                }
            }
            
            /* Check existing clients for data */
            for (size_t i = 0; i < num_clients; i++) {
                ptk_connection_state_t state = tcp_clients[i].state;
                
                if (state & PTK_CONN_DATA_READY) {
                    /* Non-blocking read - if data isn't ready immediately, skip */
                    ptk_pdu_t* request = ptk_connection_read_pdu(&eip_clients[i], scratch, 0);
                    if (request) {
                        ptk_pdu_t* response = process_request(&eip_clients[i], request, scratch);
                        if (response) {
                            /* Send with short timeout to avoid blocking server */
                            ptk_status_t send_status = ptk_connection_send_pdu(&eip_clients[i], response, 100);
                            if (send_status != PTK_OK) {
                                printf("Client %zu send failed, disconnecting\n", i);
                                state |= PTK_CONN_ERROR;
                            }
                        }
                        ptk_scratch_reset(scratch);
                    }
                }
                
                if (state & (PTK_CONN_ERROR | PTK_CONN_CLOSED)) {
                    printf("Client %zu disconnected\n", i);
                    close(tcp_clients[i].fd);
                    
                    /* Remove from arrays by shifting down */
                    memmove(&tcp_clients[i], &tcp_clients[i+1], 
                           (num_clients - i - 1) * sizeof(ptk_tcp_connection_t));
                    memmove(&eip_clients[i], &eip_clients[i+1], 
                           (num_clients - i - 1) * sizeof(ptk_eip_connection_t));
                    num_clients--;
                    i--; /* Recheck this index */
                }
            }
        }
        
        /* Handle periodic tasks, watchdogs, etc. */
        handle_periodic_tasks(num_clients);
    }
    
    /* Cleanup */
    for (size_t i = 0; i < num_clients; i++) {
        close(tcp_clients[i].fd);
    }
    close(server.fd);
    if (scratch) ptk_scratch_destroy(scratch);
    ptk_cleanup_thread_local();
    
    return 0;
}

ptk_pdu_t* process_request(ptk_eip_connection_t* conn, ptk_pdu_t* request, ptk_scratch_t* scratch) {
    /* Parse request */
    uint16_t service_code;
    if (ptk_read_uint16(request, &service_code) != PTK_OK) {
        return NULL;
    }
    
    /* Create response */
    ptk_pdu_t* response = ptk_create_pdu(scratch, 64);
    if (!response) {
        return NULL;
    }
    
    switch (service_code) {
        case 0x006F: /* Get Attribute Single */
            {
                ptk_write_uint16(response, 0x00CF); /* Reply service */
                ptk_write_uint8(response, 0x00);    /* General status: success */
                ptk_write_uint16(response, 0x1234); /* Mock attribute value */
            }
            break;
            
        default:
            {
                ptk_write_uint16(response, service_code | 0x0080); /* Error reply */
                ptk_write_uint8(response, 0x08); /* Service not supported */
            }
            break;
    }
    
    return response;
}

void handle_periodic_tasks(size_t active_connections) {
    /* Check system health, update watchdogs, etc. */
    static uint32_t last_check = 0;
    uint32_t now = get_system_time_ms();
    
    if (now - last_check > 5000) { /* Every 5 seconds */
        printf("System health check - %zu active connections\n", active_connections);
        last_check = now;
    }
}
```

### Event Connection Example - Timers and Inter-Thread Communication

```c
#include "ptk.h"
#include <pthread.h>

#define MAX_CLIENTS 8

/* Timer message structure */
typedef struct {
    uint32_t timer_id;
    uint32_t timestamp;
} timer_event_t;

/* Thread communication message */
typedef struct {
    uint8_t command;    /* 1=shutdown, 2=add_client, etc. */
    uint32_t data;
} control_event_t;

/* Timer thread function */
void* timer_thread(void* arg) {
    ptk_event_connection_t* timer_events = (ptk_event_connection_t*)arg;
    
    while (true) {
        sleep(5); /* 5 second timer */
        
        timer_event_t timer_msg = {
            .timer_id = 1,
            .timestamp = get_system_time_ms()
        };
        
        /* Signal main thread that timer expired */
        ptk_event_write(timer_events, &timer_msg, sizeof(timer_msg));
    }
    return NULL;
}

int main() {
    /* Stack allocated connections */
    ptk_tcp_connection_t server;
    ptk_tcp_connection_t tcp_clients[MAX_CLIENTS];
    ptk_eip_connection_t eip_clients[MAX_CLIENTS];
    ptk_event_connection_t timer_events;
    ptk_event_connection_t control_events;
    ptk_scratch_t* scratch = NULL;
    size_t num_clients = 0;
    bool running = true;
    pthread_t timer_tid;
    
    ptk_init_thread_local();
    
    /* Initialize connections */
    ptk_init_tcp_server(&server, 44818);
    ptk_init_event_connection(&timer_events);
    ptk_init_event_connection(&control_events);
    
    scratch = ptk_scratch_create(2048);
    memset(tcp_clients, 0, sizeof(tcp_clients));
    memset(eip_clients, 0, sizeof(eip_clients));
    
    /* Start timer thread */
    pthread_create(&timer_tid, NULL, timer_thread, &timer_events);
    
    printf("Server with timers listening on port 44818...\n");
    
    while (running) {
        /* Build event source array - clean polymorphic API */
        ptk_event_source_t* all_sources[MAX_CLIENTS + 3];
        
        all_sources[0] = (ptk_event_source_t*)&server;
        all_sources[1] = (ptk_event_source_t*)&timer_events;
        all_sources[2] = (ptk_event_source_t*)&control_events;
        
        for (size_t i = 0; i < num_clients; i++) {
            all_sources[i + 3] = (ptk_event_source_t*)&tcp_clients[i];
        }
        
        /* Wait for activity on any event source - unified API */
        int ready = ptk_wait_for_multiple(all_sources, num_clients + 3, 1000);
        
        if (ready > 0) {
            /* Check for timer events */
            if (timer_events.base.state & PTK_CONN_DATA_READY) {
                timer_event_t timer_msg;
                size_t bytes_read;
                if (ptk_event_read(&timer_events, &timer_msg, sizeof(timer_msg), &bytes_read) == PTK_OK) {
                    printf("Timer %u expired at %u ms\n", timer_msg.timer_id, timer_msg.timestamp);
                    
                    /* Send keepalive to all clients */
                    for (size_t i = 0; i < num_clients; i++) {
                        send_keepalive(&eip_clients[i], scratch);
                    }
                    ptk_scratch_reset(scratch);
                }
            }
            
            /* Check for control events */
            if (control_events.base.state & PTK_CONN_DATA_READY) {
                control_event_t control_msg;
                size_t bytes_read;
                if (ptk_event_read(&control_events, &control_msg, sizeof(control_msg), &bytes_read) == PTK_OK) {
                    switch (control_msg.command) {
                        case 1: /* Shutdown */
                            printf("Shutdown requested\n");
                            running = false;
                            break;
                        case 2: /* Force disconnect client */
                            if (control_msg.data < num_clients) {
                                printf("Force disconnecting client %u\n", control_msg.data);
                                close(tcp_clients[control_msg.data].fd);
                                tcp_clients[control_msg.data].base.state |= PTK_CONN_CLOSED;
                            }
                            break;
                    }
                }
            }
            
            /* Check server for new connections */
            if (server.base.state & PTK_CONN_DATA_READY) {
                if (num_clients < MAX_CLIENTS) {
                    ptk_status_t status = ptk_server_accept(&server, &tcp_clients[num_clients]);
                    if (status == PTK_OK) {
                        status = ptk_init_eip_connection(&eip_clients[num_clients], &tcp_clients[num_clients]);
                        if (status == PTK_OK) {
                            num_clients++;
                            printf("New client connected (%zu total)\n", num_clients);
                        } else {
                            close(tcp_clients[num_clients].fd);
                        }
                    }
                }
            }
            
            /* Check existing clients for data */
            for (size_t i = 0; i < num_clients; i++) {
                ptk_connection_state_t state = tcp_clients[i].base.state;
                
                if (state & PTK_CONN_DATA_READY) {
                    ptk_pdu_t* request = ptk_connection_read_pdu(&eip_clients[i], scratch, 0);
                    if (request) {
                        ptk_pdu_t* response = process_request(&eip_clients[i], request, scratch);
                        if (response) {
                            ptk_connection_send_pdu(&eip_clients[i], response, 100);
                        }
                        ptk_scratch_reset(scratch);
                    }
                }
                
                if (state & (PTK_CONN_ERROR | PTK_CONN_CLOSED)) {
                    printf("Client %zu disconnected\n", i);
                    close(tcp_clients[i].fd);
                    
                    /* Remove from arrays */
                    memmove(&tcp_clients[i], &tcp_clients[i+1], 
                           (num_clients - i - 1) * sizeof(ptk_tcp_connection_t));
                    memmove(&eip_clients[i], &eip_clients[i+1], 
                           (num_clients - i - 1) * sizeof(ptk_eip_connection_t));
                    num_clients--;
                    i--;
                }
            }
        }
    }
    
    /* Cleanup */
    pthread_cancel(timer_tid);
    pthread_join(timer_tid, NULL);
    
    for (size_t i = 0; i < num_clients; i++) {
        close(tcp_clients[i].fd);
    }
    close(server.fd);
    if (scratch) ptk_scratch_destroy(scratch);
    ptk_cleanup_thread_local();
    
    return 0;
}

/* Function to send shutdown signal from another thread */
void request_shutdown(ptk_event_connection_t* control_events) {
    control_event_t msg = { .command = 1, .data = 0 };
    ptk_event_write(control_events, &msg, sizeof(msg));
}

void send_keepalive(ptk_eip_connection_t* conn, ptk_scratch_t* scratch) {
    ptk_pdu_t* keepalive = ptk_create_pdu(scratch, 16);
    if (keepalive) {
        ptk_write_uint16(keepalive, 0x0065); /* NOP service */
        ptk_connection_send_pdu(conn, keepalive, 500); /* 500ms timeout */
    }
}
```

### Framing Example - Handling Partial Reads and Large PDUs

```c
#include "ptk.h"

/* Example: EtherNet/IP with large data transfers */
int main() {
    ptk_tcp_connection_t tcp_conn;
    ptk_eip_connection_t eip_conn;
    ptk_scratch_t* scratch = NULL;
    ptk_pdu_t* current_pdu = NULL;
    bool reading_partial = false;
    
    ptk_init_thread_local();
    
    /* Initialize connections */
    ptk_init_tcp_connection(&tcp_conn, "192.168.1.100", 44818);
    ptk_init_eip_connection(&eip_conn, &tcp_conn);
    
    scratch = ptk_scratch_create(8192); /* 8KB for large PDUs */
    
    while (true) {
        ptk_event_source_t* sources[] = { (ptk_event_source_t*)&tcp_conn };
        int ready = ptk_wait_for_multiple(sources, 1, 5000);
        
        if (ready > 0 && (tcp_conn.base.state & PTK_CONN_DATA_READY)) {
            
            if (!reading_partial) {
                /* Start new PDU - allocate maximum possible size */
                current_pdu = ptk_create_pdu(scratch, 4096); /* 4KB max PDU */
                if (!current_pdu) {
                    fprintf(stderr, "Failed to create PDU\n");
                    break;
                }
                reading_partial = true;
            }
            
            /* Read more data into existing PDU */
            ptk_status_t status = ptk_connection_read_into_pdu(&eip_conn, current_pdu, 100);
            
            if (status == PTK_ERROR_WOULD_BLOCK) {
                /* No more data available right now, continue waiting */
                continue;
            } else if (status != PTK_OK) {
                fprintf(stderr, "Read error: %s\n", ptk_status_string(status));
                reading_partial = false;
                ptk_scratch_reset(scratch);
                continue;
            }
            
            /* Check if we have enough data to determine expected length */
            if (current_pdu->length >= 6) {
                /* EtherNet/IP header: [Command][Length][Session Handle][Status][Context] */
                /* Length is at bytes 2-3 (big endian) */
                uint16_t msg_length = (current_pdu->data[2] << 8) | current_pdu->data[3];
                uint16_t total_length = msg_length + 24; /* + header size */
                
                /* Set the expected length - this shrinks the PDU to the actual size */
                ptk_pdu_set_length(current_pdu, total_length);
                
                printf("Expecting PDU of length %u bytes\n", total_length);
            }
            
            /* Check if PDU is complete */
            if (ptk_pdu_is_full(current_pdu)) {
                printf("Complete PDU received: %u bytes\n", current_pdu->length);
                
                /* Reset cursor to beginning for processing */
                ptk_pdu_reset_cursor(current_pdu);
                
                /* Process the complete PDU */
                process_complete_pdu(&eip_conn, current_pdu, scratch);
                
                /* Reset for next PDU */
                reading_partial = false;
                ptk_scratch_reset(scratch);
                current_pdu = NULL;
            } else {
                printf("Partial PDU: received %u of %u bytes needed\n", 
                       current_pdu->cursor, current_pdu->length);
            }
        }
    }
    
    if (scratch) ptk_scratch_destroy(scratch);
    ptk_cleanup_thread_local();
    return 0;
}

void process_complete_pdu(ptk_eip_connection_t* conn, ptk_pdu_t* pdu, ptk_scratch_t* scratch) {
    /* Parse EtherNet/IP header */
    uint16_t command, length, session_handle, status;
    
    ptk_read_uint16(pdu, &command);
    ptk_read_uint16(pdu, &length);
    ptk_read_uint32(pdu, &session_handle);
    ptk_read_uint32(pdu, &status);
    
    printf("EIP Command: 0x%04X, Length: %u, Session: 0x%08X\n", 
           command, length, session_handle);
    
    /* Process based on command type */
    switch (command) {
        case 0x0065: /* RegisterSession */
            handle_register_session(conn, pdu, scratch);
            break;
        case 0x006F: /* SendRRData */
            handle_send_rr_data(conn, pdu, scratch);
            break;
        default:
            printf("Unknown command: 0x%04X\n", command);
            break;
    }
}

/* Server example with framing support */
void handle_client_with_framing(ptk_eip_connection_t* client, ptk_scratch_t* scratch) {
    static ptk_pdu_t* client_pdu = NULL;
    static bool reading_partial = false;
    
    if (!reading_partial) {
        /* Start new PDU for this client */
        client_pdu = ptk_create_pdu(scratch, 2048); /* 2KB per client PDU */
        if (!client_pdu) return;
        reading_partial = true;
    }
    
    /* Try to read more data */
    ptk_status_t status = ptk_connection_read_into_pdu(client, client_pdu, 0); /* Non-blocking */
    
    if (status == PTK_OK) {
        /* Check for complete PDU */
        if (client_pdu->length >= 4) {
            /* Determine expected length from protocol header */
            uint16_t msg_length = (client_pdu->data[2] << 8) | client_pdu->data[3];
            ptk_pdu_set_length(client_pdu, msg_length + 24); /* + header */
        }
        
        if (ptk_pdu_is_full(client_pdu)) {
            /* Process complete message */
            ptk_pdu_reset_cursor(client_pdu);
            ptk_pdu_t* response = process_client_request(client, client_pdu, scratch);
            
            if (response) {
                ptk_connection_send_pdu(client, response, 500);
            }
            
            /* Reset for next message from this client */
            reading_partial = false;
            /* Note: don't reset scratch here - let caller do it */
        }
    } else if (status != PTK_ERROR_WOULD_BLOCK) {
        /* Error occurred, reset client state */
        reading_partial = false;
    }
}

/* Example of sending large PDU with partial writes */
void send_large_response(ptk_eip_connection_t* client, ptk_scratch_t* scratch) {
    /* Create a large response PDU */
    ptk_pdu_t* response = ptk_create_pdu(scratch, 8192); /* 8KB response */
    if (!response) return;
    
    /* Build response data using write functions */
    ptk_write_uint16(response, 0x006F); /* SendRRData */
    ptk_write_uint16(response, 8000);   /* Large data payload */
    ptk_write_uint32(response, 0x12345678); /* Session handle */
    
    /* Add 8KB of application data */
    for (int i = 0; i < 2000; i++) {
        ptk_write_uint32(response, i); /* 4 bytes * 2000 = 8000 bytes */
    }
    
    /* Set final length and prepare for sending */
    ptk_pdu_set_length(response, response->cursor); /* cursor is at end after writing */
    ptk_pdu_reset_cursor(response); /* reset cursor to 0 for sending */
    
    /* Send PDU with partial write handling */
    while (!ptk_pdu_is_sent(response)) {
        ptk_event_source_t* sources[] = { (ptk_event_source_t*)client->transport };
        int ready = ptk_wait_for_multiple(sources, 1, 1000);
        
        if (ready > 0 && (client->transport->base.state & PTK_CONN_WRITE_READY)) {
            ptk_status_t status = ptk_connection_send_pdu(client, response, 100);
            
            if (status == PTK_OK) {
                printf("Large PDU fully sent: %u bytes\n", response->length);
                break;
            } else if (status == PTK_ERROR_PARTIAL_SEND) {
                printf("Partial send: %u of %u bytes sent\n", 
                       response->cursor, response->length);
                /* Continue loop to send remaining data */
            } else if (status == PTK_ERROR_WOULD_BLOCK) {
                /* Socket not ready, continue waiting */
                continue;
            } else {
                printf("Send error: %s\n", ptk_status_string(status));
                break;
            }
        }
    }
}
```

### PDU Framing Implementation

```c
/* ptk_pdu.c - PDU framing implementation */

ptk_pdu_t* ptk_create_pdu(ptk_scratch_t* scratch, uint16_t size) {
    size_t total_size = sizeof(ptk_pdu_t) + size;
    ptk_pdu_t* pdu = (ptk_pdu_t*)ptk_scratch_alloc(scratch, total_size);
    if (!pdu) return NULL;
    
    pdu->capacity = size;
    pdu->length = 0;
    pdu->cursor = 0;
    
    return pdu;
}

bool ptk_pdu_is_full(ptk_pdu_t* pdu) {
    if (!pdu) return false;
    
    /* Full when cursor reaches length (works for both reading and writing) */
    return pdu->cursor >= pdu->length;
}

bool ptk_pdu_is_sent(ptk_pdu_t* pdu) {
    /* Same logic as ptk_pdu_is_full, just a clearer name for sending */
    return ptk_pdu_is_full(pdu);
}

ptk_status_t ptk_pdu_append_data(ptk_pdu_t* pdu, const void* data, size_t len) {
    if (!pdu || !data || len == 0) return PTK_ERROR_INVALID_PARAM;
    
    /* Check if we have space in the buffer */
    if (pdu->length + len > pdu->capacity) {
        return PTK_ERROR_BUFFER_FULL;
    }
    
    /* Append data to end of current data */
    memcpy(pdu->data + pdu->length, data, len);
    pdu->length += len;
    
    return PTK_OK;
}

void ptk_pdu_reset_cursor(ptk_pdu_t* pdu) {
    if (pdu) {
        pdu->cursor = 0;
    }
}

ptk_status_t ptk_pdu_set_length(ptk_pdu_t* pdu, uint16_t new_length) {
    if (!pdu) return PTK_ERROR_INVALID_PARAM;
    
    /* Can only set length to something smaller than current length or capacity */
    if (new_length > pdu->capacity) {
        return PTK_ERROR_BUFFER_TOO_SMALL;
    }
    
    pdu->length = new_length;
    return PTK_OK;
}

/* Read into existing PDU, appending to current length */
ptk_status_t ptk_connection_read_into_pdu(ptk_eip_connection_t* conn, 
                                         ptk_pdu_t* pdu, 
                                         uint32_t timeout_ms) {
    if (!conn || !pdu) return PTK_ERROR_INVALID_PARAM;
    
    /* Calculate how much space we have left */
    size_t available_space = pdu->capacity - pdu->length;
    if (available_space == 0) {
        return PTK_ERROR_BUFFER_FULL;
    }
    
    /* Read directly into PDU buffer at current end */
    uint8_t* read_ptr = pdu->data + pdu->length;
    
    /* Use platform-specific socket read with timeout */
    ssize_t bytes_read = ptk_pal_socket_recv(&conn->transport->pal_socket, 
                                            read_ptr, 
                                            available_space, 
                                            timeout_ms);
    
    if (bytes_read > 0) {
        pdu->length += bytes_read;
        return PTK_OK;
    } else if (bytes_read == 0) {
        return PTK_ERROR_CONNECTION_CLOSED;
    } else {
        /* Check if it's a would-block condition */
        int error = errno; /* Platform specific error handling */
        if (error == EAGAIN || error == EWOULDBLOCK) {
            return PTK_ERROR_WOULD_BLOCK;
        }
        return PTK_ERROR_READ_FAILED;
    }
}

/* Send PDU with partial write support */
ptk_status_t ptk_connection_send_pdu(ptk_eip_connection_t* conn, 
                                    ptk_pdu_t* pdu, 
                                    uint32_t timeout_ms) {
    if (!conn || !pdu) return PTK_ERROR_INVALID_PARAM;
    
    /* Calculate how much data remains to be sent */
    size_t remaining = pdu->length - pdu->cursor;
    if (remaining == 0) {
        return PTK_OK; /* Already fully sent */
    }
    
    /* Send from current cursor position */
    uint8_t* send_ptr = pdu->data + pdu->cursor;
    
    /* Use platform-specific socket send with timeout */
    ssize_t bytes_sent = ptk_pal_socket_send(&conn->transport->pal_socket, 
                                           send_ptr, 
                                           remaining, 
                                           timeout_ms);
    
    if (bytes_sent > 0) {
        pdu->cursor += bytes_sent;
        
        /* Check if we've sent everything */
        if (ptk_pdu_is_sent(pdu)) {
            return PTK_OK;
        } else {
            return PTK_ERROR_PARTIAL_SEND; /* More data to send */
        }
    } else if (bytes_sent == 0) {
        return PTK_ERROR_CONNECTION_CLOSED;
    } else {
        /* Check if it's a would-block condition */
        int error = errno; /* Platform specific error handling */
        if (error == EAGAIN || error == EWOULDBLOCK) {
            return PTK_ERROR_WOULD_BLOCK;
        }
        return PTK_ERROR_SEND_FAILED;
    }
}
```

### Framing Design Benefits

**Efficient Memory Usage:**
- Single PDU allocation handles multiple partial reads
- No copying between buffers
- Cursor remains valid across partial reads

**Protocol Flexibility:**
- Works with any protocol that has length fields
- Handles variable-length messages naturally
- Expected length can be determined dynamically

**MTU Friendly:**
- Handles 1500-byte Ethernet MTU limitations
- Gracefully accumulates data across multiple recv() calls
- No artificial size limits

**Embedded Friendly:**
- Predictable memory usage
- No hidden allocations during framing
- Reuses scratch allocator space efficiently

**Simple State Management:**
- One PDU per connection for partial reads
- Boolean flag tracks partial read state
- Clean reset when message is complete

This design lets you handle large industrial protocol messages (like EtherNet/IP file transfers) that may span multiple TCP packets while maintaining the no-hidden-allocations principle and efficient memory usage.

## Type-Safe Slice Benefits

### **Compile-Time Safety**
- Prevents mixing different data types accidentally
- Catches size calculation errors at compile time  
- Better IDE support with autocomplete for struct members
- No dangerous void* casts or manual size arithmetic

### **Industrial Protocol Advantages**
```c
/* Instead of error-prone manual parsing: */
ptk_slice_t data = get_plc_data();
uint16_t* values = (uint16_t*)data.data;  /* Dangerous cast */
size_t count = data.len / sizeof(uint16_t);  /* Manual calculation */

/* Type-safe approach: */
ptk_slice_u16_t plc_values = parse_plc_registers(data_slice, scratch);
for (size_t i = 0; i < plc_values.len; i++) {
    process_register_value(plc_values.data[i]);  /* Type-safe access */
}
```

### **Performance**
- Zero runtime overhead - all type checking at compile time
- Better compiler optimizations with known types
- Optimal memory alignment handled automatically
- No runtime type checking or validation needed

### **Maintainability**  
- Self-documenting code - type intent is clear
- Easier refactoring with compile-time type checking
- Less bug-prone than manual pointer arithmetic
- Consistent patterns across the entire codebase

## Enhanced Serialization Benefits

### **Complete Data Type Coverage**
- **Integer types**: 8, 16, 32, 64-bit unsigned integers
- **Floating point**: 32-bit float and 64-bit double precision
- **Endianness control**: Big endian, little endian, or host native
- **Industrial protocol friendly**: Covers all common data representations

### **Protocol Versatility**
```c
/* EtherNet/IP (big endian network format) */
ptk_write_uint32_be(&slice, session_handle);
ptk_write_float32_be(&slice, temperature_value);

/* Modbus TCP (mixed endianness depending on device) */
ptk_write_uint16_le(&slice, register_address);  /* Some PLCs use little endian */
ptk_write_uint16_be(&slice, register_value);    /* Others use big endian */

/* OPC UA (little endian standard) */
ptk_write_float64_le(&slice, high_precision_measurement);

/* Host processing (optimal performance) */
ptk_write_uint64(&slice, timestamp, PTK_ENDIAN_HOST);  /* No conversion overhead */
```

### **Error Handling & Safety**
- Automatic bounds checking prevents buffer overflows
- Error status returned from write operations
- Thread-local error state for read operations
- Graceful handling of insufficient buffer space

### **Performance Considerations**
- **Zero-copy**: Direct byte manipulation, no intermediate buffers
- **Host endian optimization**: `PTK_ENDIAN_HOST` uses direct memory copy
- **Compile-time optimization**: Simple switch statements optimize well  
- **Alignment aware**: Proper alignment handled in arena allocation

This comprehensive serialization system makes PTK suitable for any industrial protocol while maintaining the embedded-friendly characteristics of predictable performance and resource usage.

```

### Implementation Detail - Polymorphic Event Source Handling

```c
/* ptk_wait_for_multiple() - clean polymorphic API with timer support */
int ptk_wait_for_multiple(ptk_event_source_t** event_sources, 
                         size_t count,
                         uint32_t timeout_ms) {
    fd_set read_fds, error_fds;
    int max_fd = -1;
    struct timeval timeout;
    bool has_event_connections = false;
    bool has_timers = false;
    uint32_t current_time = ptk_get_current_time_ms();
    uint32_t next_timer_ms = UINT32_MAX;
    
    FD_ZERO(&read_fds);
    FD_ZERO(&error_fds);
    
    /* First pass: Check timers and find next expiration time */
    for (size_t i = 0; i < count; i++) {
        ptk_event_source_t* source = event_sources[i];
        
        if (source->type == PTK_EVENT_SOURCE_TIMER) {
            has_timers = true;
            ptk_timer_event_source_t* timer = (ptk_timer_event_source_t*)source;
            
            if (timer->active) {
                if (current_time >= timer->next_fire_time_ms) {
                    /* Timer has expired */
                    source->state |= PTK_CONN_DATA_READY;
                    
                    if (timer->repeating) {
                        /* Schedule next firing */
                        timer->next_fire_time_ms = current_time + timer->interval_ms;
                    } else {
                        /* One-shot timer - deactivate */
                        timer->active = false;
                    }
                } else {
                    /* Timer still running - track when it should fire */
                    uint32_t time_to_fire = timer->next_fire_time_ms - current_time;
                    if (time_to_fire < next_timer_ms) {
                        next_timer_ms = time_to_fire;
                    }
                    source->state &= ~PTK_CONN_DATA_READY;
                }
            } else {
                source->state &= ~PTK_CONN_DATA_READY;
            }
        }
    }
    
    /* Build fd_set from socket-based event sources only */
    for (size_t i = 0; i < count; i++) {
        ptk_event_source_t* source = event_sources[i];
        
        if (source->type == PTK_EVENT_SOURCE_EVENT) {
            has_event_connections = true;
            continue;
        }
        
        if (source->type == PTK_EVENT_SOURCE_TIMER) {
            continue; /* Already handled above */
        }
        
        int fd = ptk_get_source_fd(source);
        if (fd >= 0) {
            FD_SET(fd, &read_fds);
            FD_SET(fd, &error_fds);
            if (fd > max_fd) max_fd = fd;
        }
    }
    
    /* Calculate actual timeout considering timers and event connections */
    uint32_t actual_timeout = timeout_ms;
    
    if (has_timers && next_timer_ms < actual_timeout) {
        actual_timeout = next_timer_ms;
    }
    
    if (has_event_connections && actual_timeout > 50) {
        actual_timeout = 50; /* Poll event connections every 50ms */
    }
                             
    timeout.tv_sec = actual_timeout / 1000;
    timeout.tv_usec = (actual_timeout % 1000) * 1000;
    
    int socket_ready = select(max_fd + 1, &read_fds, NULL, &error_fds, &timeout);
    int total_ready = 0;
    
    /* Count expired timers first */
    for (size_t i = 0; i < count; i++) {
        ptk_event_source_t* source = event_sources[i];
        if (source->type == PTK_EVENT_SOURCE_TIMER && (source->state & PTK_CONN_DATA_READY)) {
            total_ready++;
        }
    }
    
    /* Update socket-based event source states */
    for (size_t i = 0; i < count; i++) {
        ptk_event_source_t* source = event_sources[i];
        
        if (source->type == PTK_EVENT_SOURCE_EVENT || source->type == PTK_EVENT_SOURCE_TIMER) continue;
        
        int fd = ptk_get_source_fd(source);
        ptk_connection_state_t state = 0;
        
        if (fd >= 0) {
            if (FD_ISSET(fd, &read_fds)) {
                state |= PTK_CONN_DATA_READY;
                total_ready++;
            }
            if (FD_ISSET(fd, &error_fds)) {
                state |= PTK_CONN_ERROR;
                total_ready++;
            }
        }
        
        source->state = state;
    }
    
    /* Check application event sources */
    for (size_t i = 0; i < count; i++) {
        ptk_event_source_t* source = event_sources[i];
        
        if (source->type == PTK_EVENT_SOURCE_EVENT) {
            ptk_event_connection_t* event_conn = (ptk_event_connection_t*)source;
            
            /* Check if event connection has data */
            if (ptk_event_has_data(event_conn)) {
                source->state |= PTK_CONN_DATA_READY;
                total_ready++;
            } else {
                source->state &= ~PTK_CONN_DATA_READY;
            }
        }
    }
    
    return total_ready;
}

/* Helper function to get file descriptor from event source */
static int ptk_get_source_fd(ptk_event_source_t* source) {
    switch (source->type) {
        case PTK_EVENT_SOURCE_TCP:
            return ((ptk_tcp_connection_t*)source)->fd;
        case PTK_EVENT_SOURCE_UDP:
            return ((ptk_udp_connection_t*)source)->fd;
        case PTK_EVENT_SOURCE_SERIAL:
            return ((ptk_serial_connection_t*)source)->fd;
        case PTK_EVENT_SOURCE_EVENT:
        case PTK_EVENT_SOURCE_TIMER:
            return -1; /* No file descriptor for app events or timers */
        default:
            return -1;
    }
}

/* Event connection implementation */
ptk_status_t ptk_init_event_connection(ptk_event_connection_t* conn) {
    memset(conn, 0, sizeof(*conn));
    conn->base.type = PTK_EVENT_SOURCE_EVENT;
    conn->base.state = 0;
    conn->capacity = sizeof(conn->buffer);
    return PTK_OK;
}

ptk_status_t ptk_event_write(ptk_event_connection_t* conn, const void* data, size_t len) {
    if (!conn || !data || len == 0) return PTK_ERROR_INVALID_PARAM;
    
    /* Simple circular buffer with atomic operations */
    uint16_t write_pos = conn->write_pos;
    uint16_t read_pos = conn->read_pos;
    
    /* Calculate available space */
    uint16_t available = (read_pos > write_pos) ? 
                        (read_pos - write_pos - 1) :
                        (conn->capacity - write_pos + read_pos - 1);
    
    if (len > available) {
        return PTK_ERROR_BUFFER_FULL;
    }
    
    /* Write data to buffer */
    for (size_t i = 0; i < len; i++) {
        conn->buffer[write_pos] = ((uint8_t*)data)[i];
        write_pos = (write_pos + 1) % conn->capacity;
    }
    
    /* Atomic update of write position */
    __atomic_store_n(&conn->write_pos, write_pos, __ATOMIC_RELEASE);
    
    /* Set data ready state */
    conn->base.state |= PTK_CONN_DATA_READY;
    
    return PTK_OK;
}

ptk_status_t ptk_event_read(ptk_event_connection_t* conn, void* data, size_t len, size_t* bytes_read) {
    if (!conn || !data || !bytes_read) return PTK_ERROR_INVALID_PARAM;
    
    uint16_t write_pos = __atomic_load_n(&conn->write_pos, __ATOMIC_ACQUIRE);
    uint16_t read_pos = conn->read_pos;
    
    /* Calculate available data */
    uint16_t available = (write_pos >= read_pos) ? 
                        (write_pos - read_pos) :
                        (conn->capacity - read_pos + write_pos);
    
    if (available == 0) {
        *bytes_read = 0;
        conn->base.state &= ~PTK_CONN_DATA_READY;
        return PTK_OK;
    }
    
    /* Read up to requested amount */
    size_t to_read = MIN(len, available);
    for (size_t i = 0; i < to_read; i++) {
        ((uint8_t*)data)[i] = conn->buffer[read_pos];
        read_pos = (read_pos + 1) % conn->capacity;
    }
    
    conn->read_pos = read_pos;
    *bytes_read = to_read;
    
    /* Update state if no more data */
    if (read_pos == write_pos) {
        conn->base.state &= ~PTK_CONN_DATA_READY;
    }
    
    return PTK_OK;
}

bool ptk_event_has_data(ptk_event_connection_t* conn) {
    if (!conn) return false;
    
    uint16_t write_pos = __atomic_load_n(&conn->write_pos, __ATOMIC_ACQUIRE);
    uint16_t read_pos = conn->read_pos;
    
    return write_pos != read_pos;
}

void ptk_event_clear(ptk_event_connection_t* conn) {
    if (!conn) return;
    
    conn->read_pos = __atomic_load_n(&conn->write_pos, __ATOMIC_ACQUIRE);
    conn->base.state &= ~PTK_CONN_DATA_READY;
}

/* Timer implementation */
ptk_status_t ptk_init_timer(ptk_timer_event_source_t* timer, 
                           uint32_t interval_ms, 
                           uint32_t timer_id, 
                           bool repeating) {
    if (!timer || interval_ms == 0) {
        return PTK_ERROR_INVALID_PARAM;
    }
    
    memset(timer, 0, sizeof(*timer));
    timer->base.type = PTK_EVENT_SOURCE_TIMER;
    timer->base.state = 0;
    timer->interval_ms = interval_ms;
    timer->timer_id = timer_id;
    timer->repeating = repeating;
    timer->active = false;
    timer->next_fire_time_ms = 0;
    
    return PTK_OK;
}

ptk_status_t ptk_timer_start(ptk_timer_event_source_t* timer) {
    if (!timer) {
        return PTK_ERROR_INVALID_PARAM;
    }
    
    uint32_t current_time = ptk_get_current_time_ms();
    timer->next_fire_time_ms = current_time + timer->interval_ms;
    timer->active = true;
    timer->base.state &= ~PTK_CONN_DATA_READY; /* Clear any previous expiration */
    
    return PTK_OK;
}

ptk_status_t ptk_timer_stop(ptk_timer_event_source_t* timer) {
    if (!timer) {
        return PTK_ERROR_INVALID_PARAM;
    }
    
    timer->active = false;
    timer->base.state &= ~PTK_CONN_DATA_READY;
    
    return PTK_OK;
}

ptk_status_t ptk_timer_reset(ptk_timer_event_source_t* timer) {
    if (!timer) {
        return PTK_ERROR_INVALID_PARAM;
    }
    
    if (timer->active) {
        uint32_t current_time = ptk_get_current_time_ms();
        timer->next_fire_time_ms = current_time + timer->interval_ms;
    }
    timer->base.state &= ~PTK_CONN_DATA_READY;
    
    return PTK_OK;
}

bool ptk_timer_has_expired(ptk_timer_event_source_t* timer) {
    if (!timer) return false;
    return (timer->base.state & PTK_CONN_DATA_READY) != 0;
}

uint32_t ptk_timer_get_id(ptk_timer_event_source_t* timer) {
    return timer ? timer->timer_id : 0;
}

/* Platform-specific time implementation examples */
#if defined(__linux__) || defined(__APPLE__)
#include <time.h>
uint32_t ptk_get_current_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}
#elif defined(_WIN32)
#include <windows.h>
uint32_t ptk_get_current_time_ms(void) {
    return GetTickCount();
}
#elif defined(PTK_PLATFORM_FREERTOS)
#include "FreeRTOS.h"
#include "task.h"
uint32_t ptk_get_current_time_ms(void) {
    return (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS;
}
#elif defined(PTK_PLATFORM_ZEPHYR)
#include <zephyr/kernel.h>
uint32_t ptk_get_current_time_ms(void) {
    return (uint32_t)k_uptime_get();
}
#else
/* Fallback - requires user to provide implementation */
uint32_t ptk_get_current_time_ms(void) {
    static uint32_t counter = 0;
    return ++counter; /* Not suitable for production - implement per platform */
}
#endif
```

### Design Benefits

- **Unified abstraction**: Everything is an event source with states (sockets, timers, app events)
- **Clean polymorphism**: Base `ptk_event_source_t` struct enables type-safe polymorphism in C
- **Single API**: `ptk_wait_for_multiple()` works with any event source type including timers
- **WaitForMultiple pattern**: Familiar to Windows developers, maps to select()
- **Composable protocols**: Stack protocols naturally on transports
- **Universal portability**: select() works on every POSIX system
- **Simple client code**: Just wait on one event source with timeout
- **Elegant server code**: Wait on array of mixed event sources (sockets + timers + events)
- **Performance**: Can be upgraded to epoll/kqueue while keeping same API
- **Embedded friendly**: Minimal overhead, predictable behavior
- **Event-driven integration**: Timers and inter-thread communication through unified API
- **Lock-free communication**: Atomic operations for thread-safe event passing
- **Interruptible blocking**: Any thread can wake up the main event loop
- **Integrated timers**: Native timer support without external dependencies
- **Platform-optimized timing**: Uses best available time source per platform
- **Industrial protocol ready**: Perfect for keepalives, watchdogs, and polling
- **Resource conscious**: Configurable limits, default 16 total event sources
- **Thread-local timers**: No synchronization overhead for per-thread timing
- **Type safety**: Compile-time type checking with clean casting to base type

## Cross-Platform Support

PTK supports multiple embedded operating systems through a Platform Abstraction Layer (PAL). For detailed information about cross-platform support, implementations for specific embedded OSes, and the PAL architecture, see **[ptk_pal.md](ptk_pal.md)**.

### Supported Platforms

| Platform | Event Mechanism | API Style | Status |
|----------|----------------|-----------|--------|
| **Zephyr** | `select()`/`poll()` | POSIX BSD Sockets | ✅ Native Support |
| **FreeRTOS+TCP** | Task Notifications | FreeRTOS Sockets | ✅ Polling Implementation |
| **lwIP** | `select()` | BSD Sockets | ✅ Native Support |
| **Linux/POSIX** | `select()`/`poll()`/`epoll` | POSIX | ✅ Native Support |
| **Windows** | `select()` | Winsock | ✅ Native Support |
| **VxWorks** | `select()` | POSIX | ✅ Native Support |
| **QNX Neutrino** | `select()`/`poll()` | POSIX | ✅ Native Support |

The PAL ensures that the same application code works across all supported platforms while taking advantage of platform-specific optimizations where available.
