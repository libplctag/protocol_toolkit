# Protocol Toolkit Code Generator Specification

## 1. Overview & Architecture

### Purpose
The Protocol Toolkit Code Generator (`ptk-generate`) transforms Protocol Definition Language (PDL) files into type-safe C code for parsing and generating binary protocol messages. It generates direct struct access patterns with safe array handling and bit field manipulation for industrial communication protocols.

### Implementation Language
The generator is implemented in **Python 3.8+** using modern parsing libraries and template engines for robust code generation.

### High-Level Architecture
```
PDL Files → Lexer → Parser → AST → Semantic Analysis → Code Generator → C Files
     ↓         ↓       ↓      ↓         ↓              ↓            ↓
   *.pdl   Tokens   Parse   Type    Validated      Templates    .h/.c
  imports          Tree    Info      AST          & Codegen    files
```

### Dependencies
- **Runtime Dependencies**:
  - `ptk_array.h` - Safe array types and operations
  - `ptk_alloc.h` - Memory management abstraction
  - `ptk_buf.h` - Buffer operations for I/O
  - `ptk_codec.h` - Encoding/decoding primitives
  - `ptk_err.h` - Error handling
  - `ptk_log.h` - Logging support

- **Build Dependencies**:
  - C compiler with C11 support
  - CMake 3.15+ or compatible build system

### Design Principles
- **Direct Struct Access**: Generated structs with direct field access (no vtables)
- **Type Safety**: Bounds-checked array accessors and type-specific functions
- **Memory Safety**: Allocator-based memory management with arena/malloc patterns
- **Zero-Copy Where Possible**: Minimize data copying during encode/decode
- **Protocol Agnostic**: Support for different endianness, container types, and bit patterns

## 2. Input Specification

### PDL File Format
- **File Extension**: `.pdl`
- **Character Encoding**: UTF-8
- **Comment Style**: C++ style `//` line comments
- **Case Sensitivity**: Identifiers are case-sensitive

### Configuration Options

#### Command-Line Interface
```bash
ptk-generate [OPTIONS] INPUT_FILES...

Options:
  -o, --output-dir DIR        Output directory for generated files (default: ./generated)
  -n, --namespace NAME        C namespace prefix for generated code (default: derived from filename)
  -h, --header-only           Generate header files only (no implementation)
  -v, --verbose               Enable verbose output
  -I, --include-dir DIR       Add directory to import search path
  -D, --define KEY=VALUE      Define preprocessor macro for generated code
  --max-array-size SIZE       Maximum allowed array size (default: 65536)
  --help                      Show help message
  --version                   Show version information

Examples:
  ptk-generate modbus.pdl
  ptk-generate -o ./protocols -n eip protocols/ethernet_ip.pdl
  ptk-generate -I ./common *.pdl
```

#### Configuration File (.ptk-config)
```ini
[generator]
output_dir = ./generated
namespace_prefix = proto
max_array_size = 65536

[includes]
search_paths = ./common:./protocols:../shared

[codegen]
generate_debug_info = true
optimize_small_structs = true
```

### Import Resolution
- **Search Order**:
  1. Relative to current PDL file directory
  2. Directories specified with `-I` flag
  3. Directories in configuration file
  4. Built-in system directories

- **Import Syntax**: `import namespace_name = "path/to/file.pdl"`
- **Circular Import Detection**: Generator detects and reports circular dependencies
- **Namespace Isolation**: Each imported file gets its own namespace

### Validation Rules
- **Type Consistency**: All type references must resolve to valid definitions
- **Array Bounds**: Array sizes must be positive integers or valid field references
- **Bit Field Bounds**: Bit field start_bit + length must be within source container bit width
- **Container Types**: Bit array container types must be valid primitive types
- **Field References**: Parent field references (`$.field`) must exist in parent context
- **Import Validity**: All imports must resolve to valid PDL files
- **Name Conflicts**: No duplicate definitions within same namespace
- **Expression Safety**: Length expressions must be evaluable and non-negative

## 3. Processing Pipeline

### Lexical Analysis
**Token Types:**
- **Keywords**: `def`, `import`, `type`, `fields`, `match`, `length`, `const`, `message`, `bit_field`, `source`, `container`, `start_bit`
- **Identifiers**: `[a-zA-Z_][a-zA-Z0-9_]*`
- **Numbers**: Decimal (`123`), hexadecimal (`0x1A2B`), floating point (`3.14`)
- **Strings**: Double-quoted strings with escape sequences
- **Operators**: `==`, `!=`, `<=`, `>=`, `<`, `>`, `&&`, `||`, `!`, `+`, `-`, `*`, `/`, `%`, `&`, `|`, `^`, `~`, `<<`, `>>`
- **Delimiters**: `{`, `}`, `[`, `]`, `(`, `)`, `:`, `,`, `.`, `$`
- **Comments**: `//` to end of line

### Parser
**AST Node Types:**
- **ProtocolFile**: Root node containing imports and definitions
- **ImportStatement**: Namespace and file path
- **Definition**: Name and type definition
- **TypeDefinition**: Primitive, aggregate, constant, variant array, bit array, or bit flag
- **FieldDefinition**: Field name and type specification
- **Expression**: Arithmetic/logical expressions with operator precedence
- **TypeReference**: Direct or namespaced type references

**Parsing Strategy:**
- Recursive descent parser with operator precedence climbing
- Error recovery with synchronization on statement boundaries
- Location tracking for detailed error messages

### Semantic Analysis
**Type Checking:**
- Resolve all type references to concrete definitions
- Validate array sizing expressions
- Check bit field source field compatibility and bounds
- Verify parent field references exist and are accessible

**Dependency Analysis:**
- Build dependency graph between types
- Detect circular dependencies
- Determine code generation order

**Memory Layout Analysis:**
- Calculate struct sizes and field offsets
- Identify variable-length fields that affect layout
- Optimize struct packing where possible

### Code Generation
**Template-Based Generation:**
- Jinja2-style templates for each code pattern
- Separate templates for headers (.h) and implementation (.c)
- Parameterized templates based on type characteristics

**Output Organization:**
- One header file per PDL file: `{filename}.h`
- One implementation file per PDL file: `{filename}.c`
- Common header with shared types: `{namespace}_common.h`

## 4. Output Specification

### Generated File Structure
```
output_dir/
├── {namespace}_common.h     # Shared types and includes
├── {filename1}.h            # Message type definitions
├── {filename1}.c            # Encode/decode implementations
├── {filename2}.h
├── {filename2}.c
└── ...
```

### Naming Conventions

#### Type Names
- **Message Types**: `{message_name}_t`
- **Enum Types**: `{enum_name}_t`
- **Array Types**: `{element_type}_array_t`
- **Bit Array Types**: `{container_type}_bit_array_t`

#### Function Names
- **Constructors**: `{type_name}_create(ptk_allocator_t *alloc, {type_name}_t **instance)`
- **Destructors**: `{type_name}_dispose(ptk_allocator_t *alloc, {type_name}_t *instance)`
- **Encoders**: `{type_name}_encode(ptk_allocator_t *alloc, ptk_buf_t *buf, const {type_name}_t *instance)`
- **Decoders**: `{type_name}_decode(ptk_allocator_t *alloc, {type_name}_t **instance, ptk_buf_t *buf)`
- **Array Accessors**: `{type_name}_get_{field_name}(const {type_name}_t *instance, size_t index, {element_type} *value)`
- **Bit Field Accessors**: `{type_name}_get_{field_name}(const {type_name}_t *instance)`

#### Constant Names
- **Enum Values**: `{TYPE_NAME}_{VALUE_NAME}`
- **Message Type IDs**: `{TYPE_NAME}_MESSAGE_TYPE`
- **Constants**: `{CONSTANT_NAME}` (from PDL const definitions)

### Generated Code Patterns

#### Message Struct Pattern
```c
typedef struct {message_name}_t {
    const int message_type;              // Always first field
    {field_type} {field_name};           // Direct field access
    {array_type}_array_t *{array_field}; // Arrays as safe array pointers
    {bit_field_type} {bit_field};        // Bit fields with automatic type sizing
} {message_name}_t;
```

#### Array Accessor Pattern
```c
// Bounds-checked element access
static inline ptk_err {type}_get_{field}(const {type}_t *msg, size_t index, {element_type} *value) {
    if (!msg || !msg->{field} || !value) return PTK_ERR_NULL_PTR;
    return {element_type}_array_get(msg->{field}, index, value);
}

// Bounds-checked element modification
static inline ptk_err {type}_set_{field}(const {type}_t *msg, size_t index, {element_type} value) {
    if (!msg || !msg->{field}) return PTK_ERR_NULL_PTR;
    return {element_type}_array_set(msg->{field}, index, value);
}

// Safe length access
static inline size_t {type}_get_{field}_length(const {type}_t *msg) {
    return msg && msg->{field} ? msg->{field}->len : 0;
}
```

#### Bit Field Accessor Pattern
```c
// Bit field extraction with automatic type sizing
static inline {bit_field_type} {type}_get_{field}(const {type}_t *msg) {
    return (msg->{source_container} >> {start_bit}) & ((1 << {length}) - 1);
}

static inline void {type}_set_{field}({type}_t *msg, {bit_field_type} value) {
    // Clear existing bits
    msg->{source_container} &= ~(((1 << {length}) - 1) << {start_bit});
    // Set new bits
    msg->{source_container} |= (value & ((1 << {length}) - 1)) << {start_bit};
    msg->{field} = value;
}
```

### Dependencies in Generated Code
**Required Headers:**
```c
#include "ptk_array.h"      // Safe array types
#include "ptk_allocator.h"  // Memory management
#include "ptk_buf.h"        // Buffer operations
#include "ptk_codec.h"      // Encode/decode primitives
#include "ptk_err.h"        // Error codes
#include "ptk_log.h"        // Logging macros
#include <stdint.h>         // Standard integer types
#include <stdbool.h>        // Boolean type
#include <stddef.h>         // size_t
```

## 5. Generator Implementation Details

### Template System Design
**Template Structure:**
- **Header Template**: Message declarations, type definitions, function prototypes
- **Implementation Template**: Encode/decode functions, constructors/destructors
- **Accessor Template**: Safe array and bit flag accessor functions

**Template Variables:**
- `namespace`: Code namespace prefix
- `message_types`: List of message type information
- `field_info`: Field definitions with types and metadata
- `dependencies`: Import and dependency information
- `config`: Generator configuration options

**Template Conditions:**
- Array vs. primitive field handling
- Bit array vs. bit flag generation
- Container-specific bit manipulation
- Endianness-specific encoding

### Type System Mapping

#### PDL to C Type Mapping
| PDL Type  | C Type                  | Notes                     |
| --------- | ----------------------- | ------------------------- |
| `u8`      | `uint8_t`               | 8-bit unsigned            |
| `u16`     | `uint16_t`              | 16-bit unsigned           |
| `u32`     | `uint32_t`              | 32-bit unsigned           |
| `u64`     | `uint64_t`              | 64-bit unsigned           |
| `i8`      | `int8_t`                | 8-bit signed              |
| `i16`     | `int16_t`               | 16-bit signed             |
| `i32`     | `int32_t`               | 32-bit signed             |
| `i64`     | `int64_t`               | 64-bit signed             |
| `f32`     | `float`                 | 32-bit IEEE 754           |
| `f64`     | `double`                | 64-bit IEEE 754           |
| `bit_field` | `uint8_t/uint16_t/uint32_t` | Automatically sized based on bit field length |
| `bit[]`   | `{container}_array_t *` | Container-based bit array |
| `type[]`  | `type_array_t *`        | Safe array pointer        |
| `message` | `{name}_t`              | Message struct            |

#### Endianness Handling
- **Byte Order Arrays**: `[0, 1, 2, 3]` for little-endian, `[3, 2, 1, 0]` for big-endian
- **Encode/Decode**: Use `ptk_codec` functions with endianness parameters
- **Container Types**: Endianness applies to container, not individual bits

### Memory Management Integration
**Allocator Patterns:**
- **Arena Allocator**: Bulk allocation with reset() for message lifetime
- **Malloc Allocator**: Individual allocations with free() per object
- **Custom Allocator**: User-provided allocation strategy

**Generated Allocation Code:**
```c
// Constructor pattern
ptk_err {type}_create(ptk_allocator_t *alloc, {type}_t **instance) {
    *instance = alloc->alloc(alloc, sizeof({type}_t));
    if (!*instance) return PTK_ERR_NO_RESOURCES;

    (*instance)->message_type = {TYPE}_MESSAGE_TYPE;
    // Initialize fields...
    return PTK_OK;
}

// Destructor pattern
void {type}_dispose(ptk_allocator_t *alloc, {type}_t *instance) {
    if (!instance) return;

    // Dispose array fields
    if (instance->{array_field}) {
        {element_type}_array_dispose(instance->{array_field});
        alloc->free(alloc, instance->{array_field});
    }

    alloc->free(alloc, instance);
}
```

### Error Handling Strategy
**Error Categories:**
- **Parse Errors**: Syntax errors, invalid tokens, malformed constructs
- **Semantic Errors**: Type mismatches, undefined references, circular dependencies
- **Generation Errors**: Template failures, file I/O errors, system limitations
- **Runtime Errors**: Memory allocation failures, buffer overruns, invalid data

**Error Reporting:**
- **Location Information**: File name, line number, column number
- **Context Information**: Surrounding code, expected vs. actual
- **Suggestion System**: Hints for fixing common errors
- **Error Codes**: Structured error codes for programmatic handling

**Generated Error Handling:**
```c
// All generated functions return ptk_err
// Array accessors include bounds checking
// Null pointer validation on all entry points
// Buffer overflow protection in encode/decode
```

## 6. Build Integration

### CMake Integration
```cmake
# Find the generator
find_program(PTK_GENERATE ptk-generate REQUIRED)

# Function to generate code from PDL files
function(add_ptk_protocol target_name)
    set(options HEADER_ONLY)
    set(oneValueArgs OUTPUT_DIR NAMESPACE)
    set(multiValueArgs PDL_FILES INCLUDE_DIRS)
    cmake_parse_arguments(PTK "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set(output_dir ${PTK_OUTPUT_DIR})
    if(NOT output_dir)
        set(output_dir ${CMAKE_CURRENT_BINARY_DIR}/generated)
    endif()

    set(generated_files "")
    foreach(pdl_file ${PTK_PDL_FILES})
        get_filename_component(basename ${pdl_file} NAME_WE)
        list(APPEND generated_files
             ${output_dir}/${basename}.h
             ${output_dir}/${basename}.c)
    endforeach()

    add_custom_command(
        OUTPUT ${generated_files}
        COMMAND ${PTK_GENERATE}
            --output-dir ${output_dir}
            $<$<BOOL:${PTK_NAMESPACE}>:--namespace ${PTK_NAMESPACE}>
            $<$<BOOL:${PTK_HEADER_ONLY}>:--header-only>
            $<$<BOOL:${PTK_INCLUDE_DIRS}>:-I $<JOIN:${PTK_INCLUDE_DIRS}, -I >>
            ${PTK_PDL_FILES}
        DEPENDS ${PTK_PDL_FILES}
        COMMENT "Generating protocol code from PDL files"
    )

    add_library(${target_name} ${generated_files})
    target_include_directories(${target_name} PUBLIC ${output_dir})
    target_link_libraries(${target_name} PUBLIC ptk::core)
endfunction()

# Usage:
add_ptk_protocol(modbus_protocol
    PDL_FILES protocols/modbus.pdl
    OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/protocols
    NAMESPACE modbus
)
```

### Makefile Integration
```makefile
# PDL file dependencies
PDL_FILES = protocols/modbus.pdl protocols/ethernet_ip.pdl
GENERATED_DIR = generated
PTK_GENERATE = ptk-generate

# Generated file targets
GENERATED_HEADERS = $(PDL_FILES:protocols/%.pdl=$(GENERATED_DIR)/%.h)
GENERATED_SOURCES = $(PDL_FILES:protocols/%.pdl=$(GENERATED_DIR)/%.c)
GENERATED_FILES = $(GENERATED_HEADERS) $(GENERATED_SOURCES)

# Generation rule
$(GENERATED_DIR)/%.h $(GENERATED_DIR)/%.c: protocols/%.pdl
	@mkdir -p $(GENERATED_DIR)
	$(PTK_GENERATE) --output-dir $(GENERATED_DIR) $<

# Main target depends on generated files
myprotocol: $(GENERATED_FILES)
	$(CC) -I$(GENERATED_DIR) -Iinclude $(GENERATED_SOURCES) main.c -o $@

.PHONY: clean-generated
clean-generated:
	rm -rf $(GENERATED_DIR)

clean: clean-generated
```

### Integration with Existing Projects
**Header Search Paths:**
```c
// Include generated headers
#include "modbus.h"           // Generated protocol header
#include "ethernet_ip.h"      // Generated protocol header

// Include PTK runtime
#include "ptk_array.h"        // Required by generated code
#include "ptk_allocator.h"    // Required by generated code
```

**Linking Requirements:**
- Link against Protocol Toolkit core library (`libptk_core`)
- Ensure runtime headers are in include path
- Generated code is self-contained within namespace

## 7. Example: TCP Header with Bit Fields

### PDL Definition
```pdl
def u16_be = { type: u16, byte_order: [1, 0] }
def u32_be = { type: u32, byte_order: [3, 2, 1, 0] }

def tcp_header = {
    type: message,
    fields: [
        source_port: u16_be,
        destination_port: u16_be,
        sequence_number: u32_be,
        acknowledgment_number: u32_be,
        
        // Bit field container for TCP flags
        _tcp_flags_container: u16_be,
        
        // Bit fields extracted from the container
        data_offset: { type: bit_field, source: { container: _tcp_flags_container, start_bit: 12, length: 4 } },
        reserved: { type: bit_field, source: { container: _tcp_flags_container, start_bit: 9, length: 3 } },
        ns_flag: { type: bit_field, source: { container: _tcp_flags_container, start_bit: 8, length: 1 } },
        cwr_flag: { type: bit_field, source: { container: _tcp_flags_container, start_bit: 7, length: 1 } },
        ece_flag: { type: bit_field, source: { container: _tcp_flags_container, start_bit: 6, length: 1 } },
        urg_flag: { type: bit_field, source: { container: _tcp_flags_container, start_bit: 5, length: 1 } },
        ack_flag: { type: bit_field, source: { container: _tcp_flags_container, start_bit: 4, length: 1 } },
        psh_flag: { type: bit_field, source: { container: _tcp_flags_container, start_bit: 3, length: 1 } },
        rst_flag: { type: bit_field, source: { container: _tcp_flags_container, start_bit: 2, length: 1 } },
        syn_flag: { type: bit_field, source: { container: _tcp_flags_container, start_bit: 1, length: 1 } },
        fin_flag: { type: bit_field, source: { container: _tcp_flags_container, start_bit: 0, length: 1 } },
        
        window_size: u16_be,
        checksum: u16_be,
        urgent_pointer: u16_be
    ]
}
```

### Generated C Code
```c
typedef struct tcp_header_t {
    const int message_type;
    uint16_t source_port;
    uint16_t destination_port;
    uint32_t sequence_number;
    uint32_t acknowledgment_number;
    
    uint16_t _tcp_flags_container;      // Source container
    
    // Bit fields with automatic type sizing
    uint8_t data_offset;                // 4-bit field -> uint8_t
    uint8_t reserved;                   // 3-bit field -> uint8_t
    uint8_t ns_flag;                    // 1-bit field -> uint8_t
    uint8_t cwr_flag;                   // 1-bit field -> uint8_t
    uint8_t ece_flag;                   // 1-bit field -> uint8_t
    uint8_t urg_flag;                   // 1-bit field -> uint8_t
    uint8_t ack_flag;                   // 1-bit field -> uint8_t
    uint8_t psh_flag;                   // 1-bit field -> uint8_t
    uint8_t rst_flag;                   // 1-bit field -> uint8_t
    uint8_t syn_flag;                   // 1-bit field -> uint8_t
    uint8_t fin_flag;                   // 1-bit field -> uint8_t
    
    uint16_t window_size;
    uint16_t checksum;
    uint16_t urgent_pointer;
} tcp_header_t;

// Generated bit field accessors
static inline uint8_t tcp_header_get_data_offset(const tcp_header_t *msg) {
    return (msg->_tcp_flags_container >> 12) & ((1 << 4) - 1);
}

static inline void tcp_header_set_data_offset(tcp_header_t *msg, uint8_t value) {
    msg->_tcp_flags_container &= ~(((1 << 4) - 1) << 12);
    msg->_tcp_flags_container |= (value & ((1 << 4) - 1)) << 12;
    msg->data_offset = value;
}

static inline uint8_t tcp_header_get_syn_flag(const tcp_header_t *msg) {
    return (msg->_tcp_flags_container >> 1) & ((1 << 1) - 1);
}

static inline void tcp_header_set_syn_flag(tcp_header_t *msg, uint8_t value) {
    msg->_tcp_flags_container &= ~(((1 << 1) - 1) << 1);
    msg->_tcp_flags_container |= (value & ((1 << 1) - 1)) << 1;
    msg->syn_flag = value;
}

// ... similar accessors for other bit fields
```

### Automatic Type Sizing Rules
- **1-8 bits**: `uint8_t`
- **9-16 bits**: `uint16_t`  
- **17-32 bits**: `uint32_t`
- **33-64 bits**: `uint64_t`

This specification provides the complete framework for implementing the Protocol Toolkit code generator, covering all aspects from input processing to output generation and integration with existing build systems.