#include <modbus.h>
#include <ptk_log.h>
#include <ptk_err.h>
#include <ptk_alloc.h>
#include <string.h>

//=============================================================================
// PRIVATE STRUCTURE DEFINITION
//=============================================================================

struct modbus_bit_array {
    uint8_t *bytes;           // Packed bit storage
    size_t len;               // Number of bits (not bytes)
};

//=============================================================================
// PRIVATE FUNCTIONS
//=============================================================================

/**
 * @brief Destructor for modbus bit array
 *
 * @param ptr Pointer to the bit array to clean up
 */
static void modbus_bit_array_destructor(void *ptr) {
    debug("destroying modbus bit array");
    modbus_bit_array_t *arr = (modbus_bit_array_t *)ptr;
    if (arr && arr->bytes) {
        ptk_free(&arr->bytes);
    }
}

/**
 * @brief Calculate number of bytes needed for given number of bits
 *
 * @param num_bits Number of bits
 * @return Number of bytes required
 */
static size_t bits_to_bytes(size_t num_bits) {
    return (num_bits + 7) / 8;
}

//=============================================================================
// PUBLIC FUNCTIONS
//=============================================================================

/**
 * @brief Create a new modbus bit array
 *
 * @param num_bits Number of bits in the array
 * @return Valid bit array on success, NULL on failure (check ptk_get_err())
 */
modbus_bit_array_t *modbus_bit_array_create(size_t num_bits) {
    info("creating modbus bit array with %zu bits", num_bits);
    
    if (num_bits == 0) {
        warn("attempted to create bit array with zero bits");
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return NULL;
    }

    modbus_bit_array_t *arr = ptk_alloc(sizeof(modbus_bit_array_t), modbus_bit_array_destructor);
    if (!arr) {
        error("failed to allocate bit array structure");
        return NULL;
    }

    size_t byte_count = bits_to_bytes(num_bits);
    arr->bytes = ptk_alloc(byte_count, NULL);
    if (!arr->bytes) {
        ptk_free(&arr);
        error("failed to allocate bit array storage");
        return NULL;
    }

    memset(arr->bytes, 0, byte_count);
    arr->len = num_bits;

    debug("created bit array with %zu bits (%zu bytes)", num_bits, byte_count);
    return arr;
}

/**
 * @brief Get a bit value from the array
 *
 * @param arr Bit array to read from
 * @param index Bit index (0-based)
 * @param value Output parameter for the bit value
 * @return PTK_OK on success, error code on failure
 */
ptk_err modbus_bit_array_get(const modbus_bit_array_t *arr, size_t index, bool *value) {
    if (!arr || !value) {
        warn("null parameters passed to bit array get");
        return PTK_ERR_INVALID_PARAM;
    }

    if (index >= arr->len) {
        warn("bit index %zu out of bounds (array length %zu)", index, arr->len);
        return PTK_ERR_OUT_OF_BOUNDS;
    }

    size_t byte_index = index / 8;
    size_t bit_index = index % 8;
    
    *value = (arr->bytes[byte_index] & (1 << bit_index)) != 0;
    return PTK_OK;
}

/**
 * @brief Set a bit value in the array
 *
 * @param arr Bit array to modify
 * @param index Bit index (0-based)
 * @param value Bit value to set
 * @return PTK_OK on success, error code on failure
 */
ptk_err modbus_bit_array_set(modbus_bit_array_t *arr, size_t index, bool value) {
    if (!arr) {
        warn("null bit array passed to set");
        return PTK_ERR_INVALID_PARAM;
    }

    if (index >= arr->len) {
        warn("bit index %zu out of bounds (array length %zu)", index, arr->len);
        return PTK_ERR_OUT_OF_BOUNDS;
    }

    size_t byte_index = index / 8;
    size_t bit_index = index % 8;
    
    if (value) {
        arr->bytes[byte_index] |= (1 << bit_index);
    } else {
        arr->bytes[byte_index] &= ~(1 << bit_index);
    }

    return PTK_OK;
}

/**
 * @brief Get the length of the bit array
 *
 * @param arr Bit array to query
 * @return Number of bits in the array, 0 if arr is NULL
 */
size_t modbus_bit_array_len(const modbus_bit_array_t *arr) {
    return arr ? arr->len : 0;
}

/**
 * @brief Resize a bit array
 *
 * @param arr Bit array to resize
 * @param new_len New length in bits
 * @return PTK_OK on success, error code on failure
 */
ptk_err modbus_bit_array_resize(modbus_bit_array_t *arr, size_t new_len) {
    info("resizing bit array from %zu to %zu bits", arr ? arr->len : 0, new_len);
    
    if (!arr) {
        warn("null bit array passed to resize");
        return PTK_ERR_INVALID_PARAM;
    }

    if (new_len == 0) {
        warn("attempted to resize bit array to zero length");
        return PTK_ERR_INVALID_PARAM;
    }

    size_t old_byte_count = bits_to_bytes(arr->len);
    size_t new_byte_count = bits_to_bytes(new_len);

    if (old_byte_count != new_byte_count) {
        arr->bytes = ptk_realloc(arr->bytes, new_byte_count);
        if (!arr->bytes) {
            error("failed to reallocate bit array storage");
            return PTK_ERR_NO_RESOURCES;
        }

        if (new_byte_count > old_byte_count) {
            memset(arr->bytes + old_byte_count, 0, new_byte_count - old_byte_count);
        }
    }

    arr->len = new_len;
    debug("resized bit array to %zu bits (%zu bytes)", new_len, new_byte_count);
    return PTK_OK;
}

/**
 * @brief Copy a bit array
 *
 * @param src Source bit array
 * @return Valid copy on success, NULL on failure (check ptk_get_err())
 */
modbus_bit_array_t *modbus_bit_array_copy(const modbus_bit_array_t *src) {
    if (!src) {
        warn("null source bit array passed to copy");
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return NULL;
    }

    modbus_bit_array_t *copy = modbus_bit_array_create(src->len);
    if (!copy) {
        return NULL;
    }

    size_t byte_count = bits_to_bytes(src->len);
    memcpy(copy->bytes, src->bytes, byte_count);

    debug("copied bit array with %zu bits", src->len);
    return copy;
}

/**
 * @brief Check if a bit array is valid
 *
 * @param arr Bit array to check
 * @return true if valid, false otherwise
 */
bool modbus_bit_array_is_valid(const modbus_bit_array_t *arr) {
    return arr && arr->bytes && arr->len > 0;
}

/**
 * @brief Create bit array from wire format bytes
 *
 * @param bytes Source bytes in Modbus wire format
 * @param num_bits Number of bits to extract
 * @param arr Output parameter for created bit array
 * @return PTK_OK on success, error code on failure
 */
ptk_err modbus_bit_array_from_bytes(const uint8_t *bytes, size_t num_bits, modbus_bit_array_t **arr) {
    info("creating bit array from %zu bits of wire format data", num_bits);
    
    if (!bytes || !arr || num_bits == 0) {
        warn("invalid parameters for bit array from bytes");
        return PTK_ERR_INVALID_PARAM;
    }

    modbus_bit_array_t *new_arr = modbus_bit_array_create(num_bits);
    if (!new_arr) {
        return ptk_get_err();
    }

    size_t byte_count = bits_to_bytes(num_bits);
    memcpy(new_arr->bytes, bytes, byte_count);

    *arr = new_arr;
    debug("created bit array from wire format");
    return PTK_OK;
}

/**
 * @brief Convert bit array to wire format bytes
 *
 * @param arr Source bit array
 * @param bytes Output parameter for allocated byte buffer
 * @param byte_count Output parameter for number of bytes
 * @return PTK_OK on success, error code on failure
 */
ptk_err modbus_bit_array_to_bytes(const modbus_bit_array_t *arr, uint8_t **bytes, size_t *byte_count) {
    if (!arr || !bytes || !byte_count) {
        warn("invalid parameters for bit array to bytes");
        return PTK_ERR_INVALID_PARAM;
    }

    *byte_count = bits_to_bytes(arr->len);
    *bytes = ptk_alloc(*byte_count, NULL);
    if (!*bytes) {
        error("failed to allocate output byte buffer");
        return PTK_ERR_NO_RESOURCES;
    }

    memcpy(*bytes, arr->bytes, *byte_count);
    debug("converted bit array to %zu bytes wire format", *byte_count);
    return PTK_OK;
}