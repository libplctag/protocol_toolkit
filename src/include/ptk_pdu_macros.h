#ifndef PTK_PDU_MACROS_H
#define PTK_PDU_MACROS_H

#include "ptk_serialization.h"
#include "ptk_slice.h"
#include "ptk_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * PDU Generation System using X-Macros
 * 
 * This system allows users to declare PDU structures and automatically
 * generates serialization/deserialization functions.
 * 
 * Usage:
 * 1. Declare your PDU fields using PTK_PDU_FIELDS_<name> macro
 * 2. Call PTK_DECLARE_PDU(name) to generate the PDU type and functions
 * 3. Call PTK_IMPLEMENT_PDU(name) in a source file to implement functions
 */

/**
 * Field type enumeration for PDU generation
 */
#define PTK_PDU_U8      PTK_PDU_U8
#define PTK_PDU_U16     PTK_PDU_U16  
#define PTK_PDU_U32     PTK_PDU_U32
#define PTK_PDU_U64     PTK_PDU_U64
#define PTK_PDU_S8      PTK_PDU_S8
#define PTK_PDU_S16     PTK_PDU_S16
#define PTK_PDU_S32     PTK_PDU_S32
#define PTK_PDU_S64     PTK_PDU_S64
#define PTK_PDU_F32     PTK_PDU_F32
#define PTK_PDU_F64     PTK_PDU_F64
#define PTK_PDU_BYTES   PTK_PDU_BYTES  // For variable-length byte arrays
#define PTK_PDU_CUSTOM  PTK_PDU_CUSTOM // For user-defined types

/**
 * Helper macros for field declarations
 */
#define PTK_PDU_FIELD(type, name) (type, name)

/**
 * Main PDU declaration macro - generates struct and function prototypes
 */
#define PTK_DECLARE_PDU(pdu_name) \
    /* Generate the PDU structure */ \
    typedef struct { \
        PTK_PDU_FIELDS_##pdu_name(PTK_PDU_STRUCT_FIELD) \
    } pdu_name##_t; \
    \
    /* Generate function prototypes */ \
    ptk_status_t pdu_name##_serialize(ptk_slice_bytes_t* slice, const pdu_name##_t* pdu, ptk_endian_t endian); \
    ptk_status_t pdu_name##_deserialize(ptk_slice_bytes_t* slice, pdu_name##_t* pdu, ptk_endian_t endian); \
    ptk_status_t pdu_name##_deserialize_peek(ptk_slice_bytes_t* slice, pdu_name##_t* pdu, ptk_endian_t endian); \
    size_t pdu_name##_size(const pdu_name##_t* pdu); \
    void pdu_name##_init(pdu_name##_t* pdu); \
    void pdu_name##_print(const pdu_name##_t* pdu);

/**
 * PDU implementation macro - generates actual function implementations
 */
#define PTK_IMPLEMENT_PDU(pdu_name) \
    /* Serialize function */ \
    ptk_status_t pdu_name##_serialize(ptk_slice_bytes_t* slice, const pdu_name##_t* pdu, ptk_endian_t endian) { \
        if (!slice || !pdu) return PTK_ERROR_INVALID_PARAM; \
        ptk_status_t status = PTK_OK; \
        PTK_PDU_FIELDS_##pdu_name(PTK_PDU_SERIALIZE_FIELD) \
        return status; \
    } \
    \
    /* Deserialize function */ \
    ptk_status_t pdu_name##_deserialize(ptk_slice_bytes_t* slice, pdu_name##_t* pdu, ptk_endian_t endian) { \
        if (!slice || !pdu) return PTK_ERROR_INVALID_PARAM; \
        ptk_status_t status = PTK_OK; \
        PTK_PDU_FIELDS_##pdu_name(PTK_PDU_DESERIALIZE_FIELD) \
        return status; \
    } \
    \
    /* Deserialize peek function */ \
    ptk_status_t pdu_name##_deserialize_peek(ptk_slice_bytes_t* slice, pdu_name##_t* pdu, ptk_endian_t endian) { \
        if (!slice || !pdu) return PTK_ERROR_INVALID_PARAM; \
        ptk_slice_bytes_t temp_slice = *slice; \
        ptk_status_t status = pdu_name##_deserialize(&temp_slice, pdu, endian); \
        /* Don't advance original slice on peek */ \
        return status; \
    } \
    \
    /* Size calculation function */ \
    size_t pdu_name##_size(const pdu_name##_t* pdu) { \
        (void)pdu; /* May be unused for fixed-size PDUs */ \
        size_t total_size = 0; \
        PTK_PDU_FIELDS_##pdu_name(PTK_PDU_SIZE_FIELD) \
        return total_size; \
    } \
    \
    /* Initialization function */ \
    void pdu_name##_init(pdu_name##_t* pdu) { \
        if (!pdu) return; \
        PTK_PDU_FIELDS_##pdu_name(PTK_PDU_INIT_FIELD) \
    } \
    \
    /* Debug print function */ \
    void pdu_name##_print(const pdu_name##_t* pdu) { \
        if (!pdu) { printf("NULL PDU\n"); return; } \
        printf("%s {\n", #pdu_name); \
        PTK_PDU_FIELDS_##pdu_name(PTK_PDU_PRINT_FIELD) \
        printf("}\n"); \
    }

/**
 * Field processing macros - used internally by PTK_IMPLEMENT_PDU
 */

/* Struct field generation */
#define PTK_PDU_STRUCT_FIELD(type, name) \
    PTK_PDU_TYPE_TO_C_TYPE(type) name;

/* Serialization field processing */
#define PTK_PDU_SERIALIZE_FIELD(type, name) \
    if (status == PTK_OK) { \
        *slice = PTK_PDU_WRITE_FUNC(type)(*slice, pdu->name, endian); \
        if (ptk_slice_bytes_is_empty(*slice)) status = PTK_ERROR_BUFFER_TOO_SMALL; \
    }

/* Deserialization field processing */
#define PTK_PDU_DESERIALIZE_FIELD(type, name) \
    if (status == PTK_OK) { \
        pdu->name = PTK_PDU_READ_FUNC(type)(slice, endian); \
        /* Error checking handled by individual read functions */ \
    }

/* Size calculation field processing */
#define PTK_PDU_SIZE_FIELD(type, name) \
    total_size += PTK_PDU_TYPE_SIZE(type);

/* Initialization field processing */
#define PTK_PDU_INIT_FIELD(type, name) \
    pdu->name = PTK_PDU_DEFAULT_VALUE(type);

/* Print field processing */
#define PTK_PDU_PRINT_FIELD(type, name) \
    printf("  %s: ", #name); \
    PTK_PDU_PRINT_VALUE(type, pdu->name); \
    printf("\n");

/**
 * Type mapping macros
 */

/* Map PDU types to C types */
#define PTK_PDU_TYPE_TO_C_TYPE(type) PTK_PDU_TYPE_TO_C_TYPE_##type
#define PTK_PDU_TYPE_TO_C_TYPE_PTK_PDU_U8   uint8_t
#define PTK_PDU_TYPE_TO_C_TYPE_PTK_PDU_U16  uint16_t
#define PTK_PDU_TYPE_TO_C_TYPE_PTK_PDU_U32  uint32_t
#define PTK_PDU_TYPE_TO_C_TYPE_PTK_PDU_U64  uint64_t
#define PTK_PDU_TYPE_TO_C_TYPE_PTK_PDU_S8   int8_t
#define PTK_PDU_TYPE_TO_C_TYPE_PTK_PDU_S16  int16_t
#define PTK_PDU_TYPE_TO_C_TYPE_PTK_PDU_S32  int32_t
#define PTK_PDU_TYPE_TO_C_TYPE_PTK_PDU_S64  int64_t
#define PTK_PDU_TYPE_TO_C_TYPE_PTK_PDU_F32  float
#define PTK_PDU_TYPE_TO_C_TYPE_PTK_PDU_F64  double
#define PTK_PDU_TYPE_TO_C_TYPE_PTK_PDU_BYTES ptk_slice_bytes_t

/* Map PDU types to write functions */
#define PTK_PDU_WRITE_FUNC(type) PTK_PDU_WRITE_FUNC_##type
#define PTK_PDU_WRITE_FUNC_PTK_PDU_U8   ptk_write_uint8_endian
#define PTK_PDU_WRITE_FUNC_PTK_PDU_U16  ptk_write_uint16
#define PTK_PDU_WRITE_FUNC_PTK_PDU_U32  ptk_write_uint32
#define PTK_PDU_WRITE_FUNC_PTK_PDU_U64  ptk_write_uint64
#define PTK_PDU_WRITE_FUNC_PTK_PDU_S8   ptk_write_int8_endian
#define PTK_PDU_WRITE_FUNC_PTK_PDU_S16  ptk_write_int16
#define PTK_PDU_WRITE_FUNC_PTK_PDU_S32  ptk_write_int32
#define PTK_PDU_WRITE_FUNC_PTK_PDU_S64  ptk_write_int64
#define PTK_PDU_WRITE_FUNC_PTK_PDU_F32  ptk_write_float32
#define PTK_PDU_WRITE_FUNC_PTK_PDU_F64  ptk_write_float64
#define PTK_PDU_WRITE_FUNC_PTK_PDU_BYTES ptk_write_bytes_endian

/* Map PDU types to read functions */
#define PTK_PDU_READ_FUNC(type) PTK_PDU_READ_FUNC_##type
#define PTK_PDU_READ_FUNC_PTK_PDU_U8   ptk_read_uint8_endian
#define PTK_PDU_READ_FUNC_PTK_PDU_U16  ptk_read_uint16
#define PTK_PDU_READ_FUNC_PTK_PDU_U32  ptk_read_uint32
#define PTK_PDU_READ_FUNC_PTK_PDU_U64  ptk_read_uint64
#define PTK_PDU_READ_FUNC_PTK_PDU_S8   ptk_read_int8_endian
#define PTK_PDU_READ_FUNC_PTK_PDU_S16  ptk_read_int16
#define PTK_PDU_READ_FUNC_PTK_PDU_S32  ptk_read_int32
#define PTK_PDU_READ_FUNC_PTK_PDU_S64  ptk_read_int64
#define PTK_PDU_READ_FUNC_PTK_PDU_F32  ptk_read_float32
#define PTK_PDU_READ_FUNC_PTK_PDU_F64  ptk_read_float64
#define PTK_PDU_READ_FUNC_PTK_PDU_BYTES ptk_read_bytes_endian

/* Map PDU types to sizes */
#define PTK_PDU_TYPE_SIZE(type) PTK_PDU_TYPE_SIZE_##type
#define PTK_PDU_TYPE_SIZE_PTK_PDU_U8   1
#define PTK_PDU_TYPE_SIZE_PTK_PDU_U16  2
#define PTK_PDU_TYPE_SIZE_PTK_PDU_U32  4
#define PTK_PDU_TYPE_SIZE_PTK_PDU_U64  8
#define PTK_PDU_TYPE_SIZE_PTK_PDU_S8   1
#define PTK_PDU_TYPE_SIZE_PTK_PDU_S16  2
#define PTK_PDU_TYPE_SIZE_PTK_PDU_S32  4
#define PTK_PDU_TYPE_SIZE_PTK_PDU_S64  8
#define PTK_PDU_TYPE_SIZE_PTK_PDU_F32  4
#define PTK_PDU_TYPE_SIZE_PTK_PDU_F64  8
#define PTK_PDU_TYPE_SIZE_PTK_PDU_BYTES 0  // Variable size

/* Map PDU types to default values */
#define PTK_PDU_DEFAULT_VALUE(type) PTK_PDU_DEFAULT_VALUE_##type
#define PTK_PDU_DEFAULT_VALUE_PTK_PDU_U8   0
#define PTK_PDU_DEFAULT_VALUE_PTK_PDU_U16  0
#define PTK_PDU_DEFAULT_VALUE_PTK_PDU_U32  0
#define PTK_PDU_DEFAULT_VALUE_PTK_PDU_U64  0
#define PTK_PDU_DEFAULT_VALUE_PTK_PDU_S8   0
#define PTK_PDU_DEFAULT_VALUE_PTK_PDU_S16  0
#define PTK_PDU_DEFAULT_VALUE_PTK_PDU_S32  0
#define PTK_PDU_DEFAULT_VALUE_PTK_PDU_S64  0
#define PTK_PDU_DEFAULT_VALUE_PTK_PDU_F32  0.0f
#define PTK_PDU_DEFAULT_VALUE_PTK_PDU_F64  0.0
#define PTK_PDU_DEFAULT_VALUE_PTK_PDU_BYTES (ptk_slice_bytes_t){.data = NULL, .len = 0}

/* Map PDU types to print format */
#define PTK_PDU_PRINT_VALUE(type, value) PTK_PDU_PRINT_VALUE_##type(value)
#define PTK_PDU_PRINT_VALUE_PTK_PDU_U8(v)   printf("%u", (unsigned)(v))
#define PTK_PDU_PRINT_VALUE_PTK_PDU_U16(v)  printf("%u", (unsigned)(v))
#define PTK_PDU_PRINT_VALUE_PTK_PDU_U32(v)  printf("%u", (unsigned)(v))
#define PTK_PDU_PRINT_VALUE_PTK_PDU_U64(v)  printf("%llu", (unsigned long long)(v))
#define PTK_PDU_PRINT_VALUE_PTK_PDU_S8(v)   printf("%d", (int)(v))
#define PTK_PDU_PRINT_VALUE_PTK_PDU_S16(v)  printf("%d", (int)(v))
#define PTK_PDU_PRINT_VALUE_PTK_PDU_S32(v)  printf("%d", (int)(v))
#define PTK_PDU_PRINT_VALUE_PTK_PDU_S64(v)  printf("%lld", (long long)(v))
#define PTK_PDU_PRINT_VALUE_PTK_PDU_F32(v)  printf("%.6f", (double)(v))
#define PTK_PDU_PRINT_VALUE_PTK_PDU_F64(v)  printf("%.6f", (double)(v))
#define PTK_PDU_PRINT_VALUE_PTK_PDU_BYTES(v) printf("bytes[%zu]", (v).len)

/**
 * Helper functions for 8-bit types (which don't need endianness)
 */
static inline ptk_slice_bytes_t ptk_write_uint8_endian(ptk_slice_bytes_t slice, uint8_t value, ptk_endian_t endian) {
    (void)endian; // Unused for 8-bit
    return ptk_write_uint8(slice, value);
}

static inline ptk_slice_bytes_t ptk_write_int8_endian(ptk_slice_bytes_t slice, int8_t value, ptk_endian_t endian) {
    (void)endian; // Unused for 8-bit
    return ptk_write_int8(slice, value);
}

static inline uint8_t ptk_read_uint8_endian(ptk_slice_bytes_t* slice, ptk_endian_t endian) {
    (void)endian; // Unused for 8-bit
    return ptk_read_uint8(slice);
}

static inline int8_t ptk_read_int8_endian(ptk_slice_bytes_t* slice, ptk_endian_t endian) {
    (void)endian; // Unused for 8-bit
    return ptk_read_int8(slice);
}

static inline ptk_slice_bytes_t ptk_write_bytes_endian(ptk_slice_bytes_t slice, ptk_slice_bytes_t value, ptk_endian_t endian) {
    (void)endian; // Endianness doesn't apply to byte arrays
    return ptk_write_bytes(slice, value);
}

static inline ptk_slice_bytes_t ptk_read_bytes_endian(ptk_slice_bytes_t* slice, ptk_endian_t endian) {
    (void)endian; // Endianness doesn't apply to byte arrays
    // Note: This would need to be implemented based on your ptk_read_bytes function
    // For now, this is a placeholder
    return (ptk_slice_bytes_t){.data = NULL, .len = 0};
}

#ifdef __cplusplus
}
#endif

#endif /* PTK_PDU_MACROS_H */