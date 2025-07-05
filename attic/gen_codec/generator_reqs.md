# C Struct Codec Generator Specification

This specification describes a Python program that takes an input file with C struct definitions and generates encoding and decoding functions in C.

## Allowed Input Types

The input is a subset of C structs with the following restrictions:

### Integer Types

- `codec_u8` - single byte unsigned integer
- `codec_i8` - single byte signed integer
- `codec_u16_be` - 2-byte unsigned integer in big-endian byte order
- `codec_i16_be` - 2-byte signed integer in big-endian byte order
- `codec_u16_le` - 2-byte unsigned integer in little-endian byte order
- `codec_i16_le` - 2-byte signed integer in little-endian byte order
- `codec_u32_be` - 4-byte unsigned integer in big-endian byte order
- `codec_u32_be_bs` - 4-byte unsigned integer with bytes (in the input buffer) in the order `2 3 1 0`
- `codec_i32_be` - 4-byte signed integer in big-endian byte order
- `codec_i32_be_bs` - 4-byte signed integer with bytes (in the input buffer) in the order `2 3 1 0`
- `codec_u32_le` - 4-byte unsigned integer in little-endian byte order
- `codec_u32_le_bs` - 4-byte unsigned integer with bytes in the order `1 0 3 2`
- `codec_i32_le` - 4-byte signed integer in little-endian byte order
- `codec_i32_le_bs` - 4-byte signed integer with bytes in the order `1 0 3 2`
- `codec_u64_be` - 8-byte unsigned integer in big-endian byte order (`7 6 5 4 3 2 1 0`)
- `codec_i64_be` - 8-byte signed integer in big-endian byte order
- `codec_u64_be_bs` - 8-byte unsigned integer with bytes in the order `6 7 4 5 2 3 0 1`
- `codec_i64_be_bs` - 8-byte signed integer with bytes in the order `6 7 4 5 2 3 0 1`
- `codec_u64_le` - 8-byte unsigned integer in little-endian byte order
- `codec_i64_le` - 8-byte signed integer in little-endian byte order
- `codec_u64_le_bs` - 8-byte unsigned integer with bytes in the order `1 0 3 2 5 4 7 6`
- `codec_i64_le_bs` - 8-byte signed integer with bytes in the order `1 0 3 2 5 4 7 6`

### Floating Point Types

- `codec_f32_be` - 4-byte float in big-endian byte order
- `codec_f32_be_bs` - 4-byte float in big-endian byte order with bytes in the order `2 3 1 0`
- `codec_f32_le` - 4-byte float in little-endian byte order
- `codec_f32_le_bs` - 4-byte float in little-endian byte order with bytes in the order `1 0 3 2`
- `codec_f64_be` - 8-byte double in big-endian byte order
- `codec_f64_be_bs` - 8-byte double in big-endian byte order with bytes in the order `6 7 4 5 2 3 0 1`
- `codec_f64_le` - 8-byte double in little-endian byte order
- `codec_f64_le_bs` - 8-byte double in little-endian byte order with bytes in the order `1 0 3 2 5 4 7 6`

### Other Supported Features

- Structs can embed other structs in the definition file.
- Fixed size arrays can be of any of the built in types above or other structs.
- Any pointer field can have a type, but the generator will only generate the declarations for the encode and decode functions. The user must implement them.
- The input data must be valid C code. The input is blindly copied as the start of the output .h file, unchanged.

## Output Files

The generator program generates 2 files:

### `<filename>.h`

Contains all the structs in the input file as well as declarations of functions and macros.

#### Per struct functions:
```c
codec_err_t <struct_name>_decode(struct <struct_name> **value, buf *input_buf);
codec_err_t <struct_name>_encode(buf *output_buf, const struct <struct_name> *value);
void <struct_name>_dispose(struct <struct_name> *value);
void <struct_name>_log_impl(const char *func, int line_num, ptk_log_level log_level, struct <struct_name> *value);
```

#### Per struct macros:
```c
#define <struct_name>_log_error(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_ERROR) <struct_name>_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_ERROR, value); } while(0)
#define <struct_name>_log_warn(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_WARN) <struct_name>_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_WARN, value); } while(0)
#define <struct_name>_log_info(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_INFO) <struct_name>_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_INFO, value); } while(0)
#define <struct_name>_log_debug(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_DEBUG) <struct_name>_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_DEBUG, value); } while(0)
#define <struct_name>_log_trace(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_TRACE) <struct_name>_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_TRACE, value); } while(0)
```

### `<filename>.c`

Contains per struct function implementations:
```c
codec_err_t <struct_name>_decode(struct <struct_name> **value, buf *input_buf) { ... code ...}
codec_err_t <struct_name>_encode(buf *output_buf, const struct <struct_name> *value) { ... code ...}
void <struct_name>_dispose(struct <struct_name> *value) { ... code ...}
void <struct_name>_log_impl(const char *func, int line_num, ptk_log_level log_level, struct <struct_name> *value) { ... code ...}
```

## General Requirements

- `#defines` and constants in the input file are copied directly into the output `.h` file
- The output file names are taken from the input filename without extensions. I.e. `gen_codec <filename>.def` would generate `<filename>.h` and `<filename>.c`. The extension does not matter and if missing the file will still be processed.
- The output files are completely self contained with the exception of the functions in `log.h` and `buf.h`

## Error Handling

The generated functions return `codec_err_t` which is a superset of `buf_err_t`:

```c
typedef enum {
    CODEC_OK = 0,                    // Success
    CODEC_ERR_OUT_OF_BOUNDS,         // Buffer bounds exceeded
    CODEC_ERR_NULL_PTR,              // Null pointer passed
    CODEC_ERR_NO_MEMORY,             // Memory allocation failed
    CODEC_ERR_NO_RESOURCES,          // Buffer allocation failed (maps to BUF_ERR_NO_RESOURCES)
} codec_err_t;
```

## Field Parsing

The generator program creates C code in the `.c` file to implement the decoder and encoder using format strings with the unified `buf_decode()` and `buf_encode()` API. For an embedded struct, the generator generates the decoder and encoder functions for the struct and then uses those in the decoder/encoder for the containing struct.

### Format String Generation

The generator maps codec types to format string elements:

| Codec Type        | Format String | Notes                             |
| ----------------- | ------------- | --------------------------------- |
| `codec_u8`        | `"u8"`        | Single unsigned byte              |
| `codec_i8`        | `"i8"`        | Single signed byte                |
| `codec_u16_be`    | `"> u16"`     | Big-endian 16-bit unsigned        |
| `codec_i16_be`    | `"> i16"`     | Big-endian 16-bit signed          |
| `codec_u16_le`    | `"< u16"`     | Little-endian 16-bit unsigned     |
| `codec_i16_le`    | `"< i16"`     | Little-endian 16-bit signed       |
| `codec_u32_be`    | `"> u32"`     | Big-endian 32-bit unsigned        |
| `codec_u32_be_bs` | `"> $ u32"`   | Big-endian byte-swapped 32-bit    |
| `codec_u32_le`    | `"< u32"`     | Little-endian 32-bit unsigned     |
| `codec_u32_le_bs` | `"< $ u32"`   | Little-endian byte-swapped 32-bit |
| `codec_u64_be`    | `"> u64"`     | Big-endian 64-bit unsigned        |
| `codec_u64_le`    | `"< u64"`     | Little-endian 64-bit unsigned     |
| `codec_f32_be`    | `"> f32"`     | Big-endian 32-bit float           |
| `codec_f32_le`    | `"< f32"`     | Little-endian 32-bit float        |
| `codec_f64_be`    | `"> f64"`     | Big-endian 64-bit double          |
| `codec_f64_le`    | `"< f64"`     | Little-endian 64-bit double       |

### Array Format Strings

Fixed-size arrays append the count to the base type:
- `codec_u8 data[16]` → `"16u8"`
- `codec_u16_be registers[10]` → `"> 10u16"`
- `codec_f32_le samples[4]` → `"< 4f32"`

### Generated Code Examples

For a simple struct:
```c
struct example {
    codec_u8 function_code;
    codec_u16_be address;
    codec_u32_le value;
};
```

The generator produces:
```c
// Decode implementation
codec_err_t example_decode(struct example **value, buf *input_buf) {
    *value = malloc(sizeof(struct example));
    return buf_decode(input_buf, false, "u8 > u16 < u32",
                     &(*value)->function_code,
                     &(*value)->address,
                     &(*value)->value) == BUF_OK ? CODEC_OK : CODEC_ERR_OUT_OF_BOUNDS;
}

// Encode implementation
codec_err_t example_encode(buf *output_buf, const struct example *value) {
    return buf_encode(output_buf, true, "u8 > u16 < u32",
                     value->function_code,
                     value->address,
                     value->value) == BUF_OK ? CODEC_OK : CODEC_ERR_OUT_OF_BOUNDS;
}
```

### Array Fields

Array fields are allowed as long as there is a generator-time known length. The length just needs to not be an empty string.

**Examples:**
- `codec_u8 serial_number[42]` → fixed size, OK
- `codec_u8 name[MAX_NAME_LENGTH]` → fixed size, OK, constant defined within the file. C compiler will check this.
- `codec_i32_le_bs bad_field[]` → Not OK

### Pointer Fields

Pointer fields are a signal to the generator to declare other functions and macros:

```c
codec_err_t <struct_name>_<field_name>_decode(struct <struct_name> *value, buf *input_buf);
codec_err_t <struct_name>_<field_name>_encode(buf *output_buf, const struct <struct_name> *value);
void <struct_name>_<field_name>_dispose(struct <struct_name> *value);
void <struct_name>_<field_name>_log_impl(const char *func, int line_num, ptk_log_level log_level, struct <struct_name> *value);
```

```c
#define <struct_name>_<field_name>_log_error(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_ERROR) <struct_name>_<field_name>_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_ERROR, value); } while(0)
#define <struct_name>_<field_name>_log_warn(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_WARN) <struct_name>_<field_name>_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_WARN, value); } while(0)
#define <struct_name>_<field_name>_log_info(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_INFO) <struct_name>_<field_name>_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_INFO, value); } while(0)
#define <struct_name>_<field_name>_log_debug(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_DEBUG) <struct_name>_<field_name>_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_DEBUG, value); } while(0)
#define <struct_name>_<field_name>_log_trace(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_TRACE) <struct_name>_<field_name>_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_TRACE, value); } while(0)
```

The functions are **NOT** defined in the `.c` file. It is the responsibility of the user to create those functions. This is how any variable length data is handled. Note that the arguments differ slightly from the functions generated by the generator. The parent struct pointer is passed instead of a pointer to pointer. All fields in the parent struct in lexical order before the pointer field will have been filled in. The decoder must create/allocate any data it needs and set the pointer field in the parent struct. All fields not filled in will be set to zero bytes.

**Important:** These user-defined functions use the unified format string API - they take only one buffer parameter and use `buf_decode()`/`buf_encode()` with format strings, eliminating the need for separate remaining buffer parameters.

**Buffer Lifecycle:** User-defined functions receive pre-allocated buffers and must not call `buf_alloc()` or `buf_free()` on them. The caller manages buffer allocation and deallocation. User-defined functions should only read from or write to the provided buffer using the format string API.

**Format String Usage in User-Defined Functions:** User-defined functions for pointer fields should use format strings appropriate for their data:
- For register arrays: `buf_encode(buffer, true, "> %uu16", count, reg_values)`
- For bit-packed coils: `buf_encode(buffer, true, "B%u", bit_count, coil_array)`
- For variable-length data: `buf_decode(buffer, false, "u8 &*u16", &len, &len, &data)`
- The format strings should match the expected wire format for the protocol

## Function Implementations

### Decoder Functions

The decoder functions use the unified format string API for buffer operations. The buffer cursor automatically advances as data is consumed, eliminating the need for manual offset tracking.

**Buffer Management:** The decoder functions receive allocated buffers (created with `buf_alloc()`) and do not manage buffer allocation/deallocation. Buffer lifecycle management is the responsibility of the caller.

**Format String API:** All decoding uses the unified `buf_decode()` function with printf-style format strings:
- `buf_decode(buf, false, "u8 u16 u32", &byte_val, &short_val, &int_val)` for basic types
- `buf_decode(buf, false, "> 10u16", reg_array)` for fixed arrays with endianness
- `buf_decode(buf, false, "u8 &*u16", &count, &count, &values)` for variable arrays with allocation
- `buf_decode(buf, true, "u8", &peek_val)` for lookahead without consuming data
- Format strings specify endianness: `">"` (big-endian), `"<"` (little-endian), `"="` (native)
- Array allocation indicated with `&` prefix: `"&10u16"` allocates space for 10 uint16_t values

**Generated Code Pattern:** The generator creates format strings based on struct field types:
- `codec_u8` → `"u8"`
- `codec_u16_be` → `"> u16"`
- `codec_u16_le` → `"< u16"`
- `codec_u32_be_bs` → `"> $ u32"` (byte-swapped)
- Arrays like `codec_u16_be data[10]` → `"> 10u16"`

The decoder function allocates the struct and places it in the passed pointer to a pointer, dest.

The generator will generate code to call any functions required to decode each field using appropriate format strings.

The decoder function logs the data that was used for decoding and the resulting struct data at log level `PTK_LOG_LEVEL_INFO`.

If decode fails partway through a struct with multiple fields, any previously allocated nested structs are cleaned up by calling their dispose functions. The buffer cursor position reflects how much data was successfully processed.

### Encoder Functions

The encoder functions use the unified format string API for buffer operations. The buffer cursor automatically advances as data is written, eliminating manual offset calculations.

**Buffer Management:** The encoder functions receive allocated buffers (created with `buf_alloc()`) and do not manage buffer allocation/deallocation. Buffer lifecycle management is the responsibility of the caller. If the buffer is too small, the encoder can automatically expand it by passing `expand=true` to `buf_encode()`, or return `CODEC_ERR_OUT_OF_BOUNDS` if `expand=false`.

**Format String API:** All encoding uses the unified `buf_encode()` function with printf-style format strings:
- `buf_encode(buf, false, "u8 u16 u32", byte_val, short_val, int_val)` for basic types
- `buf_encode(buf, true, "> 10u16", reg_array)` for arrays with auto-expansion and endianness
- `buf_encode(buf, false, "< u8 B16", func_code, bit_array)` for bit-packed data
- Format strings specify endianness: `">"` (big-endian), `"<"` (little-endian), `"="` (native)
- The `expand` parameter controls whether buffers grow automatically on overflow

**Generated Code Pattern:** The generator creates format strings based on struct field types:
- `codec_u8` → `"u8"`
- `codec_u16_be` → `"> u16"`
- `codec_u16_le` → `"< u16"`
- `codec_u32_be_bs` → `"> $ u32"` (byte-swapped)
- Arrays like `codec_u16_be data[10]` → `"> 10u16"`
- Bit fields get packed using `"B8"`, `"B16"`, etc.

**Buffer Space Management:**
- `buf_get_cursor()` to determine total bytes written
- `buf_len()` to check remaining space
- Automatic expansion available via `expand=true` parameter

The generator will generate code to call any functions required to encode each field using appropriate format strings.

The encoder function logs the struct data and the resulting output data bytes at log level `PTK_LOG_LEVEL_INFO`.

After successful encoding, `buf_get_cursor()` indicates the total number of bytes written to the buffer.

### Dispose Functions

The generator disposal functions call disposal functions for each field (if any) and then dispose of the memory of the struct. The dispose functions handle NULL pointers gracefully but log a warning at WARN level when called with NULL.

The disposal code logs the disposal at log level `PTK_LOG_LEVEL_INFO`.

### Log Functions

The generator creates log functions following the signature above to log out the field values of the passed structure. The logging is done one field per line using the `ptk_log_impl()` function from `log.h`. Each log entry has the field name in the log message.

The user-defined log functions are called to log the field contents.

#### Array Logging

Array elements are logged with no more than 16 elements per line for base integer and floating point types, formatted as:
```
field_name[0-15]: 0x01 0x02 0x03 ... 0x10
field_name[16-31]: 0x11 0x12 0x13 ... 0x20
```

For struct array elements, log one field per line with indices:
```
my_struct_array[0].field1: value
my_struct_array[0].field2: value
my_struct_array[1].field1: value
```

**Note:** The `log.h` functions have a hard limit of about 900 characters per log message, so care must be taken to not log too much per line.

## Output File Contents

### Header File (`.h`)

The output `.h` file contains several sections:
- Includes needed such as `log.h` and `buf.h`
- The `codec_err_t` enum definition
- The required definitions of integer and floating point data types that are "primitive" types as noted above
- The entire contents of the input definition file
- All the declarations for generated decoders, encoders, disposal functions and macros
- All declarations for user-supplied decoders, encoders, disposal functions and macros

### Implementation File (`.c`)

The output `.c` file contains:
- Any required includes
- Definitions of all generated decoders, encoders, disposal functions and macros

## Example

Given an input file, `modbus.def`, that contains the following:

```c
#define MAX_PAYLOAD 253
#define MODBUS_READ_REGS 0x03

// Simple struct with fixed fields
struct mbap {
    codec_u16_be transaction_id;    // Transaction ID
    codec_u16_be protocol_id;       /* Protocol identifier */
    codec_u16_be length;            // Length of following bytes
    codec_u8 unit_id;               // Unit identifier
};

struct read_holding_registers_req {
    codec_u8     function_code;       // Always 0x03
    codec_u16_be starting_address;
    codec_u16_be quantity_of_registers;
};

struct read_holding_registers_resp {
    codec_u8     function_code;       // Always 0x03
    codec_u8     byte_count;
    codec_u16_be *reg_values;         // User provides function.
};
```

### Generated Header (`modbus.h`)

```c
#include <stdint.h>
#include <stddef.h>
#include <log.h>
#include <buf.h>

/* Error handling enum */
typedef enum {
    CODEC_OK = 0,                    // Success
    CODEC_ERR_OUT_OF_BOUNDS,         // Buffer bounds exceeded
    CODEC_ERR_NULL_PTR,              // Null pointer passed
    CODEC_ERR_NO_MEMORY,             // Memory allocation failed
} codec_err_t;

/* always included */
typedef uint8_t codec_u8;

/* included as used in the struct definitions */
typedef uint16_t codec_u16_be;

/* entire contents of modbus.def */
#define MAX_PAYLOAD 253
#define MODBUS_READ_REGS 0x03

// Simple struct with fixed fields
struct mbap {
    codec_u16_be transaction_id;    // Transaction ID
    codec_u16_be protocol_id;       /* Protocol identifier */
    codec_u16_be length;            // Length of following bytes
    codec_u8 unit_id;               // Unit identifier
};

struct read_holding_registers_req {
    codec_u8     function_code;       // Always 0x03
    codec_u16_be starting_address;
    codec_u16_be quantity_of_registers;
};

struct read_holding_registers_resp {
    codec_u8     function_code;       // Always 0x03
    codec_u8     byte_count;
    codec_u16_be *reg_values;         // User provides function.
};

/* generated function declarations */

/* functions for struct mbap */
codec_err_t mbap_decode(struct mbap **value, buf *input_buf);
codec_err_t mbap_encode(buf *output_buf, const struct mbap *value);
void mbap_dispose(struct mbap *value);
void mbap_log_impl(const char *func, int line_num, ptk_log_level log_level, struct mbap *value);

/* functions for struct read_holding_registers_req */
codec_err_t read_holding_registers_req_decode(struct read_holding_registers_req **value, buf *input_buf);
codec_err_t read_holding_registers_req_encode(buf *output_buf, const struct read_holding_registers_req *value);
void read_holding_registers_req_dispose(struct read_holding_registers_req *value);
void read_holding_registers_req_log_impl(const char *func, int line_num, ptk_log_level log_level, struct read_holding_registers_req *value);

/* functions for struct read_holding_registers_resp */
codec_err_t read_holding_registers_resp_decode(struct read_holding_registers_resp **value, buf *input_buf);
codec_err_t read_holding_registers_resp_encode(buf *output_buf, const struct read_holding_registers_resp *value);
void read_holding_registers_resp_dispose(struct read_holding_registers_resp *value);
void read_holding_registers_resp_log_impl(const char *func, int line_num, ptk_log_level log_level, struct read_holding_registers_resp *value);

/* generated declarations for user-supplied decoder/encoder/dispose of the pointer field */
codec_err_t read_holding_registers_resp_reg_values_decode(struct read_holding_registers_resp *value, buf *input_buf);
codec_err_t read_holding_registers_resp_reg_values_encode(buf *output_buf, const struct read_holding_registers_resp *value);
void read_holding_registers_resp_reg_values_dispose(struct read_holding_registers_resp *value);
void read_holding_registers_resp_reg_values_log_impl(const char *func, int line_num, ptk_log_level log_level, struct read_holding_registers_resp *value);

/* logging macros for struct mbap */
#define mbap_log_error(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_ERROR) mbap_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_ERROR, value); } while(0)
#define mbap_log_warn(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_WARN) mbap_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_WARN, value); } while(0)
#define mbap_log_info(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_INFO) mbap_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_INFO, value); } while(0)
#define mbap_log_debug(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_DEBUG) mbap_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_DEBUG, value); } while(0)
#define mbap_log_trace(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_TRACE) mbap_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_TRACE, value); } while(0)

/* logging macros for struct read_holding_registers_req */
#define read_holding_registers_req_log_error(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_ERROR) read_holding_registers_req_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_ERROR, value); } while(0)
#define read_holding_registers_req_log_warn(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_WARN) read_holding_registers_req_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_WARN, value); } while(0)
#define read_holding_registers_req_log_info(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_INFO) read_holding_registers_req_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_INFO, value); } while(0)
#define read_holding_registers_req_log_debug(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_DEBUG) read_holding_registers_req_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_DEBUG, value); } while(0)
#define read_holding_registers_req_log_trace(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_TRACE) read_holding_registers_req_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_TRACE, value); } while(0)

/* logging macros for struct read_holding_registers_resp */
#define read_holding_registers_resp_log_error(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_ERROR) read_holding_registers_resp_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_ERROR, value); } while(0)
#define read_holding_registers_resp_log_warn(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_WARN) read_holding_registers_resp_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_WARN, value); } while(0)
#define read_holding_registers_resp_log_info(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_INFO) read_holding_registers_resp_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_INFO, value); } while(0)
#define read_holding_registers_resp_log_debug(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_DEBUG) read_holding_registers_resp_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_DEBUG, value); } while(0)
#define read_holding_registers_resp_log_trace(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_TRACE) read_holding_registers_resp_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_TRACE, value); } while(0)

/* logging macros for pointer field reg_values */
#define read_holding_registers_resp_reg_values_log_error(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_ERROR) read_holding_registers_resp_reg_values_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_ERROR, value); } while(0)
#define read_holding_registers_resp_reg_values_log_warn(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_WARN) read_holding_registers_resp_reg_values_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_WARN, value); } while(0)
#define read_holding_registers_resp_reg_values_log_info(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_INFO) read_holding_registers_resp_reg_values_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_INFO, value); } while(0)
#define read_holding_registers_resp_reg_values_log_debug(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_DEBUG) read_holding_registers_resp_reg_values_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_DEBUG, value); } while(0)
#define read_holding_registers_resp_reg_values_log_trace(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_TRACE) read_holding_registers_resp_reg_values_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_TRACE, value); } while(0)
```

### Generated Implementation (`modbus.c`)

```c
#include "modbus.h"

/* functions for struct mbap */
codec_err_t mbap_decode(struct mbap **value, buf *input_buf) {
    /* ... implementation using buf_decode() with format strings ... */
    /* ... example: buf_decode(input_buf, false, "> u16 u16 u16 u8", &(*value)->transaction_id, &(*value)->protocol_id, &(*value)->length, &(*value)->unit_id) ... */
    /* ... automatic cursor management, no manual offsets ... */
    /* ... input_buf must be pre-allocated with buf_alloc() by caller ... */
    /* ... function does not call buf_free() on input_buf ... */
}

codec_err_t mbap_encode(buf *output_buf, const struct mbap *value) {
    /* ... implementation using buf_encode() with format strings ... */
    /* ... example: buf_encode(output_buf, true, "> u16 u16 u16 u8", value->transaction_id, value->protocol_id, value->length, value->unit_id) ... */
    /* ... automatic cursor management, no manual offsets ... */
    /* ... output_buf must be pre-allocated with buf_alloc() by caller ... */
    /* ... function does not call buf_free() on output_buf ... */
    /* ... may return CODEC_ERR_OUT_OF_BOUNDS if buffer too small and expand=false ... */
}

void mbap_dispose(struct mbap *value) {
    /* ... implementation code ... */
}

void mbap_log_impl(const char *func, int line_num, ptk_log_level log_level, struct mbap *value) {
    /* ... implementation code ... */
}

/* functions for struct read_holding_registers_req */
codec_err_t read_holding_registers_req_decode(struct read_holding_registers_req **value, buf *input_buf) {
    /* ... implementation using cursor-based buf API ... */
}

codec_err_t read_holding_registers_req_encode(buf *output_buf, const struct read_holding_registers_req *value) {
    /* ... implementation using cursor-based buf API ... */
}

void read_holding_registers_req_dispose(struct read_holding_registers_req *value) {
    /* ... implementation code ... */
}

void read_holding_registers_req_log_impl(const char *func, int line_num, ptk_log_level log_level, struct read_holding_registers_req *value) {
    /* ... implementation code ... */
}

/* functions for struct read_holding_registers_resp */
codec_err_t read_holding_registers_resp_decode(struct read_holding_registers_resp **value, buf *input_buf) {
    /* ... implementation using cursor-based buf API ... */
    /* ... call to read_holding_registers_resp_reg_values_decode() with single buffer ... */
    /* ... implementation code ... */
}

codec_err_t read_holding_registers_resp_encode(buf *output_buf, const struct read_holding_registers_resp *value) {
    /* ... implementation using cursor-based buf API ... */
    /* ... call to read_holding_registers_resp_reg_values_encode() with single buffer ... */
    /* ... implementation code ... */
}

void read_holding_registers_resp_dispose(struct read_holding_registers_resp *value) {
    /* ... implementation code ... */
    /* ... call to read_holding_registers_resp_reg_values_dispose() ... */
    /* ... implementation code ... */
}

void read_holding_registers_resp_log_impl(const char *func, int line_num, ptk_log_level log_level, struct read_holding_registers_resp *value) {
    /* ... implementation code ... */
}
```
