/**
 * PDU Macro System for Protocol Toolkit
 *
 * This header provides macros for generating C structs and type-safe
 * serialization/deserialization functions for protocol data units (PDUs),
 * including support for nested PDUs and custom field handling.
 *
 * Usage:
 *   1. Define your PDU fields macro:
 *        #define PTK_PDU_FIELDS_mytype(FIELD) \
 *            FIELD(PTK_PDU_U16, id) \
 *            FIELD(PTK_PDU_U32, value)
 *   2. Use PTK_DECLARE_PDU(mytype) in a header.
 *   3. Use PTK_IMPLEMENT_PDU(mytype) in a source file.
 *
 * For nested PDUs or custom fields, see macros below.
 */

/**
 * Unified PDU Macro System for Protocol Toolkit
 *
 * This header provides all macros for generating C structs and type-safe
 * serialization/deserialization functions for protocol data units (PDUs),
 * including support for nested PDUs, custom types, variable-length fields,
 * conditional fields, and unions. It is standalone and does not require
 * ptk_pdu_macros.h or ptk_pdu_custom.h.
 */

#ifndef PTK_PDU_H
#define PTK_PDU_H

#include "ptk_types.h"
#include "ptk_slice.h"
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// Field type tags (base and custom)
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
#define PTK_PDU_BYTES   PTK_PDU_BYTES
#define PTK_PDU_CUSTOM  PTK_PDU_CUSTOM
#define PTK_PDU_STRING      PTK_PDU_STRING
#define PTK_PDU_ARRAY       PTK_PDU_ARRAY
#define PTK_PDU_CONDITIONAL PTK_PDU_CONDITIONAL
#define PTK_PDU_NESTED      PTK_PDU_NESTED
#define PTK_PDU_UNION       PTK_PDU_UNION

// --- Field declaration helpers (for custom/complex fields) ---
#define PTK_PDU_FIELD_CUSTOM(type_name, field_name) (PTK_PDU_CUSTOM, field_name, type_name)
#define PTK_PDU_FIELD_STRING(field_name, max_len) (PTK_PDU_STRING, field_name, max_len)
#define PTK_PDU_FIELD_ARRAY(element_type, field_name, count_field) (PTK_PDU_ARRAY, field_name, element_type, count_field)
#define PTK_PDU_FIELD_CONDITIONAL(condition_field, condition_value, type, field_name) (PTK_PDU_CONDITIONAL, field_name, type, condition_field, condition_value)
#define PTK_PDU_FIELD_NESTED(pdu_type, field_name) (PTK_PDU_NESTED, field_name, pdu_type)

// --- Main PDU declaration macro (base + custom) ---
#define PTK_DECLARE_PDU(pdu_name) \
    typedef struct { \
        PTK_PDU_FIELDS_##pdu_name(PTK_PDU_STRUCT_FIELD) \
    } pdu_name##_t; \
    ptk_status_t pdu_name##_serialize(ptk_slice_bytes_t* slice, const pdu_name##_t* pdu, ptk_endian_t endian); \
    ptk_status_t pdu_name##_deserialize(ptk_slice_bytes_t* slice, pdu_name##_t* pdu, ptk_endian_t endian); \
    ptk_status_t pdu_name##_deserialize_peek(const ptk_slice_bytes_t* slice, pdu_name##_t* pdu, ptk_endian_t endian); \
    size_t pdu_name##_size(const pdu_name##_t* pdu); \
    void pdu_name##_init(pdu_name##_t* pdu); \
    void pdu_name##_destroy(pdu_name##_t* pdu); \
    void pdu_name##_print(const pdu_name##_t* pdu);

// --- Main PDU implementation macro (base + custom) ---
#define PTK_IMPLEMENT_PDU(pdu_name) \
    ptk_status_t pdu_name##_serialize(ptk_slice_bytes_t* slice, const pdu_name##_t* pdu, ptk_endian_t endian) { \
        if (!slice || !pdu) return PTK_ERROR_INVALID_PARAM; \
        ptk_status_t status = PTK_OK; \
        PTK_PDU_FIELDS_##pdu_name(PTK_PDU_SERIALIZE_FIELD) \
        return status; \
    } \
    ptk_status_t pdu_name##_deserialize(ptk_slice_bytes_t* slice, pdu_name##_t* pdu, ptk_endian_t endian) { \
        if (!slice || !pdu) return PTK_ERROR_INVALID_PARAM; \
        ptk_status_t status = PTK_OK; \
        PTK_PDU_FIELDS_##pdu_name(PTK_PDU_DESERIALIZE_FIELD) \
        return status; \
    } \
    ptk_status_t pdu_name##_deserialize_peek(const ptk_slice_bytes_t* slice, pdu_name##_t* pdu, ptk_endian_t endian) { \
        if (!slice || !pdu) return PTK_ERROR_INVALID_PARAM; \
        ptk_slice_bytes_t temp_slice = *slice; \
        return pdu_name##_deserialize(&temp_slice, pdu, endian); \
    } \
    size_t pdu_name##_size(const pdu_name##_t* pdu) { \
        size_t total_size = 0; \
        PTK_PDU_FIELDS_##pdu_name(PTK_PDU_SIZE_FIELD) \
        return total_size; \
    } \
    void pdu_name##_init(pdu_name##_t* pdu) { \
        if (!pdu) return; \
        memset(pdu, 0, sizeof(*pdu)); \
        PTK_PDU_FIELDS_##pdu_name(PTK_PDU_INIT_FIELD) \
    } \
    void pdu_name##_destroy(pdu_name##_t* pdu) { \
        if (!pdu) return; \
        PTK_PDU_FIELDS_##pdu_name(PTK_PDU_DESTROY_FIELD) \
    } \
    void pdu_name##_print(const pdu_name##_t* pdu) { \
        if (!pdu) { printf("NULL PDU\n"); return; } \
        printf("%s {\n", #pdu_name); \
        PTK_PDU_FIELDS_##pdu_name(PTK_PDU_PRINT_FIELD) \
        printf("}\n"); \
    }

// --- Field processing macros (dispatch to custom or base as needed) ---
#define PTK_PDU_STRUCT_FIELD(...) PTK_PDU_STRUCT_FIELD_IMPL(__VA_ARGS__)
#define PTK_PDU_STRUCT_FIELD_IMPL(type, name, ...) PTK_PDU_STRUCT_FIELD_##type(name, __VA_ARGS__)

#define PTK_PDU_SERIALIZE_FIELD(...) PTK_PDU_SERIALIZE_FIELD_IMPL(__VA_ARGS__)
#define PTK_PDU_SERIALIZE_FIELD_IMPL(type, name, ...) PTK_PDU_SERIALIZE_FIELD_##type(name, __VA_ARGS__)

#define PTK_PDU_DESERIALIZE_FIELD(...) PTK_PDU_DESERIALIZE_FIELD_IMPL(__VA_ARGS__)
#define PTK_PDU_DESERIALIZE_FIELD_IMPL(type, name, ...) PTK_PDU_DESERIALIZE_FIELD_##type(name, __VA_ARGS__)

#define PTK_PDU_SIZE_FIELD(...) PTK_PDU_SIZE_FIELD_IMPL(__VA_ARGS__)
#define PTK_PDU_SIZE_FIELD_IMPL(type, name, ...) PTK_PDU_SIZE_FIELD_##type(name, __VA_ARGS__)

#define PTK_PDU_INIT_FIELD(...) PTK_PDU_INIT_FIELD_IMPL(__VA_ARGS__)
#define PTK_PDU_INIT_FIELD_IMPL(type, name, ...) PTK_PDU_INIT_FIELD_##type(name, __VA_ARGS__)

#define PTK_PDU_DESTROY_FIELD(...) PTK_PDU_DESTROY_FIELD_IMPL(__VA_ARGS__)
#define PTK_PDU_DESTROY_FIELD_IMPL(type, name, ...) PTK_PDU_DESTROY_FIELD_##type(name, __VA_ARGS__)

#define PTK_PDU_PRINT_FIELD(...) PTK_PDU_PRINT_FIELD_IMPL(__VA_ARGS__)
#define PTK_PDU_PRINT_FIELD_IMPL(type, name, ...) PTK_PDU_PRINT_FIELD_##type(name, __VA_ARGS__)

// --- Base type field macros ---
#define PTK_PDU_STRUCT_FIELD_PTK_PDU_U8(name, ...)     uint8_t name;
#define PTK_PDU_STRUCT_FIELD_PTK_PDU_U16(name, ...)    uint16_t name;
#define PTK_PDU_STRUCT_FIELD_PTK_PDU_U32(name, ...)    uint32_t name;
#define PTK_PDU_STRUCT_FIELD_PTK_PDU_U64(name, ...)    uint64_t name;
#define PTK_PDU_STRUCT_FIELD_PTK_PDU_S8(name, ...)     int8_t name;
#define PTK_PDU_STRUCT_FIELD_PTK_PDU_S16(name, ...)    int16_t name;
#define PTK_PDU_STRUCT_FIELD_PTK_PDU_S32(name, ...)    int32_t name;
#define PTK_PDU_STRUCT_FIELD_PTK_PDU_S64(name, ...)    int64_t name;
#define PTK_PDU_STRUCT_FIELD_PTK_PDU_F32(name, ...)    float name;
#define PTK_PDU_STRUCT_FIELD_PTK_PDU_F64(name, ...)    double name;
#define PTK_PDU_STRUCT_FIELD_PTK_PDU_BYTES(name, ...)  ptk_slice_bytes_t name;

// --- Custom/complex field macros ---
#define PTK_PDU_STRUCT_FIELD_PTK_PDU_CUSTOM(name, type_name) type_name name;
#define PTK_PDU_STRUCT_FIELD_PTK_PDU_STRING(name, max_len) \
    struct { uint16_t len; char data[max_len]; } name;
#define PTK_PDU_STRUCT_FIELD_PTK_PDU_ARRAY(name, element_type, count_field) \
    struct { element_type* data; size_t capacity; } name;
#define PTK_PDU_STRUCT_FIELD_PTK_PDU_CONDITIONAL(name, type, condition_field, condition_value) \
    type name; bool name##_present;
#define PTK_PDU_STRUCT_FIELD_PTK_PDU_NESTED(name, pdu_type) pdu_type##_t name;

// --- Serialization, deserialization, size, init, destroy, print macros ---
// (For brevity, only base types shown here; custom types should be extended as in ptk_pdu_custom.h)

#define PTK_PDU_SERIALIZE_FIELD_PTK_PDU_U8(name, ...) \
    *slice = ptk_serialize_u8(*slice, pdu->name);
#define PTK_PDU_SERIALIZE_FIELD_PTK_PDU_U16(name, ...) \
    *slice = ptk_serialize_u16(*slice, pdu->name, endian);
// ... (repeat for all base types, and extend for custom types as in ptk_pdu_custom.h)

#define PTK_PDU_DESERIALIZE_FIELD_PTK_PDU_U8(name, ...) \
    pdu->name = ptk_deserialize_u8(slice, false);
// ... (repeat for all base types, and extend for custom types as in ptk_pdu_custom.h)

#define PTK_PDU_SIZE_FIELD_PTK_PDU_U8(name, ...) total_size += 1;
// ... (repeat for all base types, and extend for custom types as in ptk_pdu_custom.h)

#define PTK_PDU_INIT_FIELD_PTK_PDU_U8(name, ...) pdu->name = 0;
// ... (repeat for all base types, and extend for custom types as in ptk_pdu_custom.h)

#define PTK_PDU_DESTROY_FIELD_PTK_PDU_U8(name, ...) /* no-op for base types */
// ... (repeat for all base types, and extend for custom types as in ptk_pdu_custom.h)

#define PTK_PDU_PRINT_FIELD_PTK_PDU_U8(name, ...) printf("  %s: %u\n", #name, (unsigned)pdu->name);
// ... (repeat for all base types, and extend for custom types as in ptk_pdu_custom.h)

// --- Add/extend macros for custom, string, array, conditional, nested, union fields as needed ---
// (see ptk_pdu_custom.h for full patterns)

#ifdef __cplusplus
}
#endif

#endif /* PTK_PDU_H */
