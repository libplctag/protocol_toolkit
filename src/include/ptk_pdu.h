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
    ptk_status_t pdu_name##_serialize(ptk_slice_bytes_t* slice, const pdu_name##_t* pdu); \
    ptk_status_t pdu_name##_deserialize(ptk_slice_bytes_t* slice, pdu_name##_t **pdu, ptk_scratch_t *scratch, bool peek); \
    size_t pdu_name##_size(const pdu_name##_t* pdu); \
    void pdu_name##_init(pdu_name##_t* pdu); \
    void pdu_name##_print(const pdu_name##_t* pdu);

// --- Main PDU implementation macro (base + custom) ---
#define PTK_IMPLEMENT_PDU(pdu_name) \
    ptk_status_t pdu_name##_serialize(ptk_slice_bytes_t* slice, const pdu_name##_t* pdu) { \
        if (!slice || !pdu) return PTK_ERROR_INVALID_PARAM; \
        ptk_status_t status = PTK_OK; \
        PTK_PDU_FIELDS_##pdu_name(PTK_PDU_SERIALIZE_FIELD) \
        return status; \
    } \
    ptk_status_t pdu_name##_deserialize(ptk_slice_bytes_t* slice, pdu_name##_t **pdu, ptk_scratch_t *scratch, bool peek) { \
        if (!slice || !pdu) return PTK_ERROR_INVALID_PARAM; \
        ptk_status_t status = PTK_OK; \
        PTK_PDU_FIELDS_##pdu_name(PTK_PDU_DESERIALIZE_FIELD) \
        return status; \
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
// Base types (unchanged)
#define PTK_PDU_SERIALIZE_FIELD_PTK_PDU_U8(name, ...) \
    do { \
        if ((slice)->len < 1) return PTK_ERROR_BUFFER_TOO_SMALL; \
        *slice = ptk_serialize_u8(*slice, pdu->name); \
    } while(0)
#define PTK_PDU_SERIALIZE_FIELD_PTK_PDU_U16(name, ...) \
    do { \
        if ((slice)->len < 2) return PTK_ERROR_BUFFER_TOO_SMALL; \
        *slice = ptk_serialize_u16(*slice, pdu->name, endian); \
    } while(0)
// ... (repeat for all base types)
// Deserialization for base types with peek support
#define PTK_PDU_DESERIALIZE_FIELD_PTK_PDU_U8(name, ...) \
    do { \
        ptk_slice_bytes_t *_slice_ptr = (peek ? (ptk_slice_bytes_t[]){*slice} : slice); \
        if (_slice_ptr->len < 1) return PTK_ERROR_BUFFER_TOO_SMALL; \
        pdu->name = ptk_deserialize_u8(_slice_ptr, peek); \
        if (!peek) *slice = *_slice_ptr; \
    } while(0)
#define PTK_PDU_SIZE_FIELD_PTK_PDU_U8(name, ...) total_size += 1;
#define PTK_PDU_INIT_FIELD_PTK_PDU_U8(name, ...) pdu->name = 0;
#define PTK_PDU_DESTROY_FIELD_PTK_PDU_U8(name, ...) /* no-op for base types */
#define PTK_PDU_PRINT_FIELD_PTK_PDU_U8(name, ...) printf("  %s: %u\n", #name, (unsigned)pdu->name);

// --- Custom/complex field macros ---
// Custom type: expects type_name_{serialize,deserialize,size,init,destroy,print}
#define PTK_PDU_SERIALIZE_FIELD_PTK_PDU_CUSTOM(name, type_name) \
    status = type_name##_serialize(slice, &pdu->name); if(status != PTK_OK) return status;
#define PTK_PDU_DESERIALIZE_FIELD_PTK_PDU_CUSTOM(name, type_name) \
    status = type_name##_deserialize(slice, &pdu->name, scratch, peek); if(status != PTK_OK) return status;
#define PTK_PDU_SIZE_FIELD_PTK_PDU_CUSTOM(name, type_name) \
    total_size += type_name##_size(&pdu->name);
#define PTK_PDU_INIT_FIELD_PTK_PDU_CUSTOM(name, type_name) \
    type_name##_init(&pdu->name);
#define PTK_PDU_DESTROY_FIELD_PTK_PDU_CUSTOM(name, type_name) \
    type_name##_destroy(&pdu->name);
#define PTK_PDU_PRINT_FIELD_PTK_PDU_CUSTOM(name, type_name) \
    type_name##_print(&pdu->name);

// String: C string, null-terminated, no length word
#define PTK_PDU_SERIALIZE_FIELD_PTK_PDU_STRING(name, max_len) \
    do { \
        size_t _len = strnlen(pdu->name.data, max_len); \
        if ((slice)->len < _len + 1) return PTK_ERROR_BUFFER_TOO_SMALL; \
        memcpy(slice->data, pdu->name.data, _len); \
        slice->data[_len] = '\0'; \
        *slice = ptk_slice_bytes_advance(*slice, _len + 1); \
    } while(0)
#define PTK_PDU_DESERIALIZE_FIELD_PTK_PDU_STRING(name, max_len) \
    do { \
        ptk_slice_bytes_t *_slice_ptr = (peek ? (ptk_slice_bytes_t[]){*slice} : slice); \
        size_t _i = 0; \
        while(_i < max_len && _i < _slice_ptr->len && _slice_ptr->data[_i] != '\0') { \
            pdu->name.data[_i] = _slice_ptr->data[_i]; \
            _i++; \
        } \
        if (_i == _slice_ptr->len) return PTK_ERROR_BUFFER_TOO_SMALL; \
        pdu->name.data[_i] = '\0'; \
        if (!peek) *slice = ptk_slice_bytes_advance(*slice, _i + 1); \
    } while(0)
#define PTK_PDU_SIZE_FIELD_PTK_PDU_STRING(name, max_len) \
    total_size += strnlen(pdu->name.data, max_len) + 1;
#define PTK_PDU_INIT_FIELD_PTK_PDU_STRING(name, max_len) \
    pdu->name.data[0] = '\0';
#define PTK_PDU_DESTROY_FIELD_PTK_PDU_STRING(name, max_len) /* no-op */
#define PTK_PDU_PRINT_FIELD_PTK_PDU_STRING(name, max_len) \
    printf("  %s: '%s'\n", #name, pdu->name.data);

// Array: count_expr is a C expression for the number of elements
#define PTK_PDU_SERIALIZE_FIELD_PTK_PDU_ARRAY(name, element_type, count_expr) \
    do { \
        size_t _count = (count_expr); \
        size_t _bytes_needed = sizeof(element_type) * _count; \
        if ((slice)->len < _bytes_needed) return PTK_ERROR_BUFFER_TOO_SMALL; \
        for(size_t _i = 0; _i < _count; ++_i) { \
            *slice = ptk_serialize_##element_type(*slice, pdu->name.data[_i]); \
        } \
    } while(0)
#define PTK_PDU_DESERIALIZE_FIELD_PTK_PDU_ARRAY(name, element_type, count_expr) \
    do { \
        size_t _count = (count_expr); \
        size_t _bytes_needed = sizeof(element_type) * _count; \
        ptk_slice_bytes_t *_slice_ptr = (peek ? (ptk_slice_bytes_t[]){*slice} : slice); \
        if (_slice_ptr->len < _bytes_needed) return PTK_ERROR_BUFFER_TOO_SMALL; \
        pdu->name.data = (element_type*)ptk_scratch_alloc(scratch, sizeof(element_type) * _count).data; \
        pdu->name.capacity = _count; \
        for(size_t _i = 0; _i < _count; ++_i) { \
            pdu->name.data[_i] = ptk_deserialize_##element_type(_slice_ptr, peek); \
        } \
        if (!peek) *slice = *_slice_ptr; \
    } while(0)
#define PTK_PDU_SIZE_FIELD_PTK_PDU_ARRAY(name, element_type, count_expr) \
    total_size += sizeof(element_type) * (count_expr);
#define PTK_PDU_INIT_FIELD_PTK_PDU_ARRAY(name, element_type, count_expr) \
    pdu->name.data = NULL; pdu->name.capacity = 0;
#define PTK_PDU_DESTROY_FIELD_PTK_PDU_ARRAY(name, element_type, count_expr) /* no-op, arena managed */
#define PTK_PDU_PRINT_FIELD_PTK_PDU_ARRAY(name, element_type, count_expr) \
    do { \
        size_t _count = (count_expr); \
        printf("  %s: [", #name); \
        for(size_t _i = 0; _i < _count; ++_i) { \
            printf("%d", (int)pdu->name.data[_i]); \
            if(_i + 1 < _count) printf(", "); \
        } \
        printf("]\n"); \
    } while(0)

// Conditional: only present if (condition_expr)
#define PTK_PDU_SERIALIZE_FIELD_PTK_PDU_CONDITIONAL(name, type, condition_expr, condition_value) \
    if(condition_expr) { PTK_PDU_SERIALIZE_FIELD_##type(name); }
#define PTK_PDU_DESERIALIZE_FIELD_PTK_PDU_CONDITIONAL(name, type, condition_expr, condition_value) \
    if(condition_expr) { PTK_PDU_DESERIALIZE_FIELD_##type(name); pdu->name##_present = true; } else { pdu->name##_present = false; }
#define PTK_PDU_SIZE_FIELD_PTK_PDU_CONDITIONAL(name, type, condition_expr, condition_value) \
    if(condition_expr) { PTK_PDU_SIZE_FIELD_##type(name); }
#define PTK_PDU_INIT_FIELD_PTK_PDU_CONDITIONAL(name, type, condition_expr, condition_value) \
    pdu->name##_present = false; PTK_PDU_INIT_FIELD_##type(name);
#define PTK_PDU_DESTROY_FIELD_PTK_PDU_CONDITIONAL(name, type, condition_expr, condition_value) \
    if(pdu->name##_present) { PTK_PDU_DESTROY_FIELD_##type(name); }
#define PTK_PDU_PRINT_FIELD_PTK_PDU_CONDITIONAL(name, type, condition_expr, condition_value) \
    if(pdu->name##_present) { PTK_PDU_PRINT_FIELD_##type(name); }

// Nested PDU
#define PTK_PDU_SERIALIZE_FIELD_PTK_PDU_NESTED(name, pdu_type) \
    status = pdu_type##_serialize(slice, &pdu->name); if(status != PTK_OK) return status;
#define PTK_PDU_DESERIALIZE_FIELD_PTK_PDU_NESTED(name, pdu_type) \
    status = pdu_type##_deserialize(slice, &pdu->name, scratch, peek); if(status != PTK_OK) return status;
#define PTK_PDU_SIZE_FIELD_PTK_PDU_NESTED(name, pdu_type) \
    total_size += pdu_type##_size(&pdu->name);
#define PTK_PDU_INIT_FIELD_PTK_PDU_NESTED(name, pdu_type) \
    pdu_type##_init(&pdu->name);
#define PTK_PDU_DESTROY_FIELD_PTK_PDU_NESTED(name, pdu_type) \
    pdu_type##_destroy(&pdu->name);
#define PTK_PDU_PRINT_FIELD_PTK_PDU_NESTED(name, pdu_type) \
    pdu_type##_print(&pdu->name);

// Union: expects a tag field to select the active member
#define PTK_PDU_SERIALIZE_FIELD_PTK_PDU_UNION(name, tag_type, tag_field, ...) \
    switch(pdu->tag_field) { __VA_ARGS__ } // User must provide case macros
#define PTK_PDU_DESERIALIZE_FIELD_PTK_PDU_UNION(name, tag_type, tag_field, ...) \
    switch(pdu->tag_field) { __VA_ARGS__ } // User must provide case macros
#define PTK_PDU_SIZE_FIELD_PTK_PDU_UNION(name, tag_type, tag_field, ...) \
    switch(pdu->tag_field) { __VA_ARGS__ }
#define PTK_PDU_INIT_FIELD_PTK_PDU_UNION(name, tag_type, tag_field, ...) \
    /* User must provide init for each case */
#define PTK_PDU_DESTROY_FIELD_PTK_PDU_UNION(name, tag_type, tag_field, ...) \
    switch(pdu->tag_field) { __VA_ARGS__ }
#define PTK_PDU_PRINT_FIELD_PTK_PDU_UNION(name, tag_type, tag_field, ...) \
    switch(pdu->tag_field) { __VA_ARGS__ }

#ifdef __cplusplus
}
#endif

#endif /* PTK_PDU_H */
