#include "../include/modbus.h"
#include <string.h>

//=============================================================================
// BIT ARRAY IMPLEMENTATION
//=============================================================================

static void modbus_bit_array_dispose(modbus_bit_array_t *arr) {
    // No explicit cleanup needed - ptk_free will handle the bytes array
    // since it's allocated as a child of the bit array
    (void)arr;
}

modbus_bit_array_t *modbus_bit_array_create(void *parent, size_t num_bits) {
    if(num_bits == 0) { return NULL; }

    modbus_bit_array_t *arr = ptk_alloc(parent, sizeof(modbus_bit_array_t), (void (*)(void *))modbus_bit_array_dispose);
    if(!arr) { return NULL; }

    size_t byte_count = (num_bits + 7) / 8;  // Round up division
    arr->bytes = ptk_alloc(arr, byte_count, NULL);
    if(!arr->bytes) {
        ptk_free(arr);
        return NULL;
    }

    arr->len = num_bits;

    // Initialize all bits to 0
    memset(arr->bytes, 0, byte_count);

    return arr;
}

ptk_err modbus_bit_array_get(const modbus_bit_array_t *arr, size_t index, bool *value) {
    if(!arr || !value) { return PTK_ERR_NULL_PTR; }
    if(index >= arr->len) { return PTK_ERR_OUT_OF_BOUNDS; }

    size_t byte_index = index / 8;
    size_t bit_index = index % 8;

    *value = (arr->bytes[byte_index] & (1 << bit_index)) != 0;
    return PTK_OK;
}

ptk_err modbus_bit_array_set(modbus_bit_array_t *arr, size_t index, bool value) {
    if(!arr) { return PTK_ERR_NULL_PTR; }
    if(index >= arr->len) { return PTK_ERR_OUT_OF_BOUNDS; }

    size_t byte_index = index / 8;
    size_t bit_index = index % 8;

    if(value) {
        arr->bytes[byte_index] |= (1 << bit_index);
    } else {
        arr->bytes[byte_index] &= ~(1 << bit_index);
    }

    return PTK_OK;
}

size_t modbus_bit_array_len(const modbus_bit_array_t *arr) { return arr ? arr->len : 0; }

ptk_err modbus_bit_array_resize(modbus_bit_array_t *arr, size_t new_len) {
    if(!arr) { return PTK_ERR_NULL_PTR; }
    if(new_len == 0) { return PTK_ERR_INVALID_PARAM; }

    size_t old_byte_count = (arr->len + 7) / 8;
    size_t new_byte_count = (new_len + 7) / 8;

    if(old_byte_count != new_byte_count) {
        uint8_t *new_bytes = ptk_realloc(arr->bytes, new_byte_count);
        if(!new_bytes) { return PTK_ERR_NO_RESOURCES; }

        // If expanding, initialize new bytes to 0
        if(new_byte_count > old_byte_count) { memset(new_bytes + old_byte_count, 0, new_byte_count - old_byte_count); }

        arr->bytes = new_bytes;
    }

    arr->len = new_len;
    return PTK_OK;
}

modbus_bit_array_t *modbus_bit_array_copy(const modbus_bit_array_t *src, void *parent) {
    if(!src || src->len == 0) { return NULL; }

    modbus_bit_array_t *dst = modbus_bit_array_create(parent, src->len);
    if(!dst) { return NULL; }

    size_t byte_count = (src->len + 7) / 8;
    memcpy(dst->bytes, src->bytes, byte_count);

    return dst;
}

bool modbus_bit_array_is_valid(const modbus_bit_array_t *arr) { return (arr && arr->len > 0 && arr->bytes != NULL); }

ptk_err modbus_bit_array_from_bytes(void *parent, const uint8_t *bytes, size_t num_bits, modbus_bit_array_t **arr) {
    if(!bytes || !arr || num_bits == 0) { return PTK_ERR_NULL_PTR; }

    *arr = modbus_bit_array_create(parent, num_bits);
    if(!*arr) { return PTK_ERR_NO_RESOURCES; }

    size_t byte_count = (num_bits + 7) / 8;
    memcpy((*arr)->bytes, bytes, byte_count);

    return PTK_OK;
}

ptk_err modbus_bit_array_to_bytes(const modbus_bit_array_t *arr, uint8_t **bytes, size_t *byte_count) {
    if(!arr || !bytes || !byte_count) { return PTK_ERR_NULL_PTR; }
    if(!modbus_bit_array_is_valid(arr)) { return PTK_ERR_INVALID_PARAM; }

    *byte_count = (arr->len + 7) / 8;
    *bytes = arr->bytes;  // Direct pointer to internal storage

    return PTK_OK;
}
