#ifndef PTK_PDU_CUSTOM_H
#define PTK_PDU_CUSTOM_H

#include "ptk_pdu_macros.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Extended PDU System with Custom Type Support
 * 
 * This extends the basic PDU system to support:
 * - User-defined custom types
 * - Variable-length fields
 * - Conditional fields
 * - EtherNet/IP style complex structures
 */

/**
 * Custom field types for complex protocols
 */
#define PTK_PDU_STRING      PTK_PDU_STRING      // Length-prefixed string
#define PTK_PDU_ARRAY       PTK_PDU_ARRAY       // Variable-length array
#define PTK_PDU_CONDITIONAL PTK_PDU_CONDITIONAL // Conditional field
#define PTK_PDU_NESTED      PTK_PDU_NESTED      // Nested PDU
#define PTK_PDU_UNION       PTK_PDU_UNION       // Union/variant type

/**
 * Custom field declaration macros with additional parameters
 */
#define PTK_PDU_FIELD_CUSTOM(type_name, field_name) (PTK_PDU_CUSTOM, field_name, type_name)
#define PTK_PDU_FIELD_STRING(field_name, max_len) (PTK_PDU_STRING, field_name, max_len)
#define PTK_PDU_FIELD_ARRAY(element_type, field_name, count_field) (PTK_PDU_ARRAY, field_name, element_type, count_field)
#define PTK_PDU_FIELD_CONDITIONAL(condition_field, condition_value, type, field_name) (PTK_PDU_CONDITIONAL, field_name, type, condition_field, condition_value)
#define PTK_PDU_FIELD_NESTED(pdu_type, field_name) (PTK_PDU_NESTED, field_name, pdu_type)

/**
 * Extended PDU declaration macro with custom type support
 */
#define PTK_DECLARE_PDU_CUSTOM(pdu_name) \
    /* Generate the PDU structure */ \
    typedef struct { \
        PTK_PDU_FIELDS_##pdu_name(PTK_PDU_STRUCT_FIELD_CUSTOM) \
    } pdu_name##_t; \
    \
    /* Generate function prototypes */ \
    ptk_status_t pdu_name##_serialize(ptk_slice_bytes_t* slice, const pdu_name##_t* pdu, ptk_endian_t endian); \
    ptk_status_t pdu_name##_deserialize(ptk_slice_bytes_t* slice, pdu_name##_t* pdu, ptk_endian_t endian); \
    ptk_status_t pdu_name##_deserialize_peek(ptk_slice_bytes_t* slice, pdu_name##_t* pdu, ptk_endian_t endian); \
    size_t pdu_name##_size(const pdu_name##_t* pdu); \
    void pdu_name##_init(pdu_name##_t* pdu); \
    void pdu_name##_destroy(pdu_name##_t* pdu); \
    void pdu_name##_print(const pdu_name##_t* pdu);

/**
 * Extended PDU implementation macro with custom type support
 */
#define PTK_IMPLEMENT_PDU_CUSTOM(pdu_name) \
    /* Serialize function */ \
    ptk_status_t pdu_name##_serialize(ptk_slice_bytes_t* slice, const pdu_name##_t* pdu, ptk_endian_t endian) { \
        if (!slice || !pdu) return PTK_ERROR_INVALID_PARAM; \
        ptk_status_t status = PTK_OK; \
        PTK_PDU_FIELDS_##pdu_name(PTK_PDU_SERIALIZE_FIELD_CUSTOM) \
        return status; \
    } \
    \
    /* Deserialize function */ \
    ptk_status_t pdu_name##_deserialize(ptk_slice_bytes_t* slice, pdu_name##_t* pdu, ptk_endian_t endian) { \
        if (!slice || !pdu) return PTK_ERROR_INVALID_PARAM; \
        ptk_status_t status = PTK_OK; \
        PTK_PDU_FIELDS_##pdu_name(PTK_PDU_DESERIALIZE_FIELD_CUSTOM) \
        return status; \
    } \
    \
    /* Deserialize peek function */ \
    ptk_status_t pdu_name##_deserialize_peek(ptk_slice_bytes_t* slice, pdu_name##_t* pdu, ptk_endian_t endian) { \
        if (!slice || !pdu) return PTK_ERROR_INVALID_PARAM; \
        ptk_slice_bytes_t temp_slice = *slice; \
        ptk_status_t status = pdu_name##_deserialize(&temp_slice, pdu, endian); \
        return status; \
    } \
    \
    /* Size calculation function */ \
    size_t pdu_name##_size(const pdu_name##_t* pdu) { \
        if (!pdu) return 0; \
        size_t total_size = 0; \
        PTK_PDU_FIELDS_##pdu_name(PTK_PDU_SIZE_FIELD_CUSTOM) \
        return total_size; \
    } \
    \
    /* Initialization function */ \
    void pdu_name##_init(pdu_name##_t* pdu) { \
        if (!pdu) return; \
        PTK_PDU_FIELDS_##pdu_name(PTK_PDU_INIT_FIELD_CUSTOM) \
    } \
    \
    /* Cleanup function */ \
    void pdu_name##_destroy(pdu_name##_t* pdu) { \
        if (!pdu) return; \
        PTK_PDU_FIELDS_##pdu_name(PTK_PDU_DESTROY_FIELD_CUSTOM) \
    } \
    \
    /* Debug print function */ \
    void pdu_name##_print(const pdu_name##_t* pdu) { \
        if (!pdu) { printf("NULL PDU\n"); return; } \
        printf("%s {\n", #pdu_name); \
        PTK_PDU_FIELDS_##pdu_name(PTK_PDU_PRINT_FIELD_CUSTOM) \
        printf("}\n"); \
    }

/**
 * Custom field processing macros
 */

/* Struct field generation for custom types */
#define PTK_PDU_STRUCT_FIELD_CUSTOM(...) PTK_PDU_STRUCT_FIELD_CUSTOM_IMPL(__VA_ARGS__)
#define PTK_PDU_STRUCT_FIELD_CUSTOM_IMPL(type, name, ...) \
    PTK_PDU_STRUCT_FIELD_CUSTOM_##type(name, __VA_ARGS__)

/* Basic types */
#define PTK_PDU_STRUCT_FIELD_CUSTOM_PTK_PDU_U8(name, ...)     uint8_t name;
#define PTK_PDU_STRUCT_FIELD_CUSTOM_PTK_PDU_U16(name, ...)    uint16_t name;
#define PTK_PDU_STRUCT_FIELD_CUSTOM_PTK_PDU_U32(name, ...)    uint32_t name;
#define PTK_PDU_STRUCT_FIELD_CUSTOM_PTK_PDU_U64(name, ...)    uint64_t name;
#define PTK_PDU_STRUCT_FIELD_CUSTOM_PTK_PDU_S8(name, ...)     int8_t name;
#define PTK_PDU_STRUCT_FIELD_CUSTOM_PTK_PDU_S16(name, ...)    int16_t name;
#define PTK_PDU_STRUCT_FIELD_CUSTOM_PTK_PDU_S32(name, ...)    int32_t name;
#define PTK_PDU_STRUCT_FIELD_CUSTOM_PTK_PDU_S64(name, ...)    int64_t name;
#define PTK_PDU_STRUCT_FIELD_CUSTOM_PTK_PDU_F32(name, ...)    float name;
#define PTK_PDU_STRUCT_FIELD_CUSTOM_PTK_PDU_F64(name, ...)    double name;

/* Custom types */
#define PTK_PDU_STRUCT_FIELD_CUSTOM_PTK_PDU_CUSTOM(name, type_name) type_name name;
#define PTK_PDU_STRUCT_FIELD_CUSTOM_PTK_PDU_STRING(name, max_len) \
    struct { uint16_t len; char data[max_len]; } name;
#define PTK_PDU_STRUCT_FIELD_CUSTOM_PTK_PDU_ARRAY(name, element_type, count_field) \
    struct { element_type* data; size_t capacity; } name;
#define PTK_PDU_STRUCT_FIELD_CUSTOM_PTK_PDU_CONDITIONAL(name, type, condition_field, condition_value) \
    type name; bool name##_present;
#define PTK_PDU_STRUCT_FIELD_CUSTOM_PTK_PDU_NESTED(name, pdu_type) pdu_type##_t name;

/**
 * Serialization field processing for custom types
 */
#define PTK_PDU_SERIALIZE_FIELD_CUSTOM(...) PTK_PDU_SERIALIZE_FIELD_CUSTOM_IMPL(__VA_ARGS__)
#define PTK_PDU_SERIALIZE_FIELD_CUSTOM_IMPL(type, name, ...) \
    if (status == PTK_OK) { \
        PTK_PDU_SERIALIZE_FIELD_CUSTOM_##type(name, __VA_ARGS__) \
    }

/* Basic type serialization */
#define PTK_PDU_SERIALIZE_FIELD_CUSTOM_PTK_PDU_U8(name, ...) \
    *slice = ptk_write_uint8(*slice, pdu->name);
#define PTK_PDU_SERIALIZE_FIELD_CUSTOM_PTK_PDU_U16(name, ...) \
    *slice = ptk_write_uint16(*slice, pdu->name, endian);
#define PTK_PDU_SERIALIZE_FIELD_CUSTOM_PTK_PDU_U32(name, ...) \
    *slice = ptk_write_uint32(*slice, pdu->name, endian);
#define PTK_PDU_SERIALIZE_FIELD_CUSTOM_PTK_PDU_U64(name, ...) \
    *slice = ptk_write_uint64(*slice, pdu->name, endian);
#define PTK_PDU_SERIALIZE_FIELD_CUSTOM_PTK_PDU_S8(name, ...) \
    *slice = ptk_write_int8(*slice, pdu->name);
#define PTK_PDU_SERIALIZE_FIELD_CUSTOM_PTK_PDU_S16(name, ...) \
    *slice = ptk_write_int16(*slice, pdu->name, endian);
#define PTK_PDU_SERIALIZE_FIELD_CUSTOM_PTK_PDU_S32(name, ...) \
    *slice = ptk_write_int32(*slice, pdu->name, endian);
#define PTK_PDU_SERIALIZE_FIELD_CUSTOM_PTK_PDU_S64(name, ...) \
    *slice = ptk_write_int64(*slice, pdu->name, endian);
#define PTK_PDU_SERIALIZE_FIELD_CUSTOM_PTK_PDU_F32(name, ...) \
    *slice = ptk_write_float32(*slice, pdu->name, endian);
#define PTK_PDU_SERIALIZE_FIELD_CUSTOM_PTK_PDU_F64(name, ...) \
    *slice = ptk_write_float64(*slice, pdu->name, endian);

/* Custom type serialization */
#define PTK_PDU_SERIALIZE_FIELD_CUSTOM_PTK_PDU_CUSTOM(name, type_name) \
    status = type_name##_serialize(slice, &pdu->name, endian);

#define PTK_PDU_SERIALIZE_FIELD_CUSTOM_PTK_PDU_STRING(name, max_len) \
    *slice = ptk_write_uint16(*slice, pdu->name.len, endian); \
    if (ptk_slice_bytes_is_empty(*slice)) { status = PTK_ERROR_BUFFER_TOO_SMALL; } \
    else { \
        ptk_slice_bytes_t str_slice = ptk_slice_bytes_make((uint8_t*)pdu->name.data, pdu->name.len); \
        *slice = ptk_write_bytes(*slice, str_slice); \
        if (ptk_slice_bytes_is_empty(*slice)) status = PTK_ERROR_BUFFER_TOO_SMALL; \
    }

#define PTK_PDU_SERIALIZE_FIELD_CUSTOM_PTK_PDU_ARRAY(name, element_type, count_field) \
    /* Array serialization would need element-specific logic */ \
    status = PTK_ERROR_INVALID_PARAM; /* Placeholder */

#define PTK_PDU_SERIALIZE_FIELD_CUSTOM_PTK_PDU_CONDITIONAL(name, type, condition_field, condition_value) \
    if (pdu->condition_field == condition_value) { \
        pdu->name##_present = true; \
        /* Serialize the field based on its type */ \
        status = PTK_ERROR_INVALID_PARAM; /* Need type-specific logic */ \
    }

#define PTK_PDU_SERIALIZE_FIELD_CUSTOM_PTK_PDU_NESTED(name, pdu_type) \
    status = pdu_type##_serialize(slice, &pdu->name, endian);

/**
 * Similar macros needed for deserialization, size calculation, etc.
 * (Abbreviated for brevity - full implementation would include all operations)
 */

/**
 * Registry system for custom types
 */
typedef struct {
    const char* type_name;
    ptk_status_t (*serialize)(ptk_slice_bytes_t* slice, const void* obj, ptk_endian_t endian);
    ptk_status_t (*deserialize)(ptk_slice_bytes_t* slice, void* obj, ptk_endian_t endian);
    size_t (*size)(const void* obj);
    void (*init)(void* obj);
    void (*destroy)(void* obj);
    void (*print)(const void* obj);
} ptk_custom_type_registry_t;

/**
 * Custom type registration macro
 */
#define PTK_REGISTER_CUSTOM_TYPE(type_name) \
    static const ptk_custom_type_registry_t type_name##_registry = { \
        .type_name = #type_name, \
        .serialize = (ptk_status_t(*)(ptk_slice_bytes_t*, const void*, ptk_endian_t))type_name##_serialize, \
        .deserialize = (ptk_status_t(*)(ptk_slice_bytes_t*, void*, ptk_endian_t))type_name##_deserialize, \
        .size = (size_t(*)(const void*))type_name##_size, \
        .init = (void(*)(void*))type_name##_init, \
        .destroy = (void(*)(void*))type_name##_destroy, \
        .print = (void(*)(const void*))type_name##_print \
    };

/**
 * Helper functions for common patterns
 */

/* Variable-length string helpers */
typedef struct {
    uint16_t len;
    char* data;
    size_t capacity;
} ptk_vstring_t;

ptk_status_t ptk_vstring_serialize(ptk_slice_bytes_t* slice, const ptk_vstring_t* str, ptk_endian_t endian);
ptk_status_t ptk_vstring_deserialize(ptk_slice_bytes_t* slice, ptk_vstring_t* str, ptk_endian_t endian);
size_t ptk_vstring_size(const ptk_vstring_t* str);
void ptk_vstring_init(ptk_vstring_t* str, size_t capacity);
void ptk_vstring_destroy(ptk_vstring_t* str);
void ptk_vstring_print(const ptk_vstring_t* str);

/* Dynamic array helpers */
typedef struct {
    void* data;
    size_t count;
    size_t capacity;
    size_t element_size;
} ptk_darray_t;

ptk_status_t ptk_darray_serialize(ptk_slice_bytes_t* slice, const ptk_darray_t* arr, ptk_endian_t endian,
                                  ptk_status_t (*element_serialize)(ptk_slice_bytes_t*, const void*, ptk_endian_t));
ptk_status_t ptk_darray_deserialize(ptk_slice_bytes_t* slice, ptk_darray_t* arr, ptk_endian_t endian,
                                    ptk_status_t (*element_deserialize)(ptk_slice_bytes_t*, void*, ptk_endian_t));

#ifdef __cplusplus
}
#endif

#endif /* PTK_PDU_CUSTOM_H */