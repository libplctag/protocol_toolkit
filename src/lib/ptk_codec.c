#include "ptk_codec.h"
#include "ptk_buf.h"
#include "ptk_log.h"
#include <string.h>
#include <arpa/inet.h>  // For htons, htonl, etc.

//=============================================================================
// INTERNAL HELPERS
//=============================================================================

static inline u16 codec_swap_u16(u16 value) {
    return ((value & 0xFF) << 8) | ((value >> 8) & 0xFF);
}

static inline u32 codec_swap_u32(u32 value) {
    return ((value & 0xFF) << 24) | 
           (((value >> 8) & 0xFF) << 16) | 
           (((value >> 16) & 0xFF) << 8) | 
           ((value >> 24) & 0xFF);
}

static inline u64 codec_swap_u64(u64 value) {
    return ((value & 0xFF) << 56) |
           (((value >> 8) & 0xFF) << 48) |
           (((value >> 16) & 0xFF) << 40) |
           (((value >> 24) & 0xFF) << 32) |
           (((value >> 32) & 0xFF) << 24) |
           (((value >> 40) & 0xFF) << 16) |
           (((value >> 48) & 0xFF) << 8) |
           ((value >> 56) & 0xFF);
}

static u16 apply_u16_endianness(u16 value, ptk_codec_endianness_t endianness) {
    switch (endianness) {
        case PTK_CODEC_BIG_ENDIAN:
            return htons(value);
        case PTK_CODEC_BIG_ENDIAN_BYTE_SWAP:
            return codec_swap_u16(htons(value));
        case PTK_CODEC_LITTLE_ENDIAN:
            return value;  // Native host order on little endian
        case PTK_CODEC_LITTLE_ENDIAN_BYTE_SWAP:
            return codec_swap_u16(value);
        default:
            return value;
    }
}

static u32 apply_u32_endianness(u32 value, ptk_codec_endianness_t endianness) {
    switch (endianness) {
        case PTK_CODEC_BIG_ENDIAN:
            return htonl(value);
        case PTK_CODEC_BIG_ENDIAN_BYTE_SWAP:
            return codec_swap_u32(htonl(value));
        case PTK_CODEC_LITTLE_ENDIAN:
            return value;  // Native host order on little endian
        case PTK_CODEC_LITTLE_ENDIAN_BYTE_SWAP:
            return codec_swap_u32(value);
        default:
            return value;
    }
}

static u64 apply_u64_endianness(u64 value, ptk_codec_endianness_t endianness) {
    switch (endianness) {
        case PTK_CODEC_BIG_ENDIAN:
            // No standard htonll, do manual conversion
            return ((u64)htonl((u32)value) << 32) | htonl((u32)(value >> 32));
        case PTK_CODEC_BIG_ENDIAN_BYTE_SWAP:
            return codec_swap_u64(((u64)htonl((u32)value) << 32) | htonl((u32)(value >> 32)));
        case PTK_CODEC_LITTLE_ENDIAN:
            return value;  // Native host order on little endian
        case PTK_CODEC_LITTLE_ENDIAN_BYTE_SWAP:
            return codec_swap_u64(value);
        default:
            return value;
    }
}

//=============================================================================
// BUFFER ENCODING FUNCTIONS
//=============================================================================

ptk_err ptk_codec_produce_u8(ptk_buf_t *buf, u8 value) {
    if (!buf || !buf->data) {
        return PTK_ERR_NULL_PTR;
    }

    // Check if we have space to write
    size_t available_space;
    ptk_err err = ptk_buf_get_remaining(&available_space, buf);
    if (err != PTK_OK) return err;
    
    if (available_space < 1) {
        // Try to resize the underlying array
        size_t new_size = buf->data->len + 1;
        err = u8_array_resize(buf->data, new_size);
        if (err != PTK_OK) {
            error("Failed to resize buffer for u8 write");
            return err;
        }
    }

    // Write the byte
    buf->data->elements[buf->end] = value;
    buf->end++;
    
    trace("Produced u8: 0x%02X", value);
    return PTK_OK;
}

ptk_err ptk_codec_produce_u16(ptk_buf_t *buf, u16 value, ptk_codec_endianness_t endianness) {
    if (!buf || !buf->data) {
        return PTK_ERR_NULL_PTR;
    }

    u16 encoded_value = apply_u16_endianness(value, endianness);
    u8 *bytes = (u8 *)&encoded_value;

    for (int i = 0; i < 2; i++) {
        ptk_err err = ptk_codec_produce_u8(buf, bytes[i]);
        if (err != PTK_OK) return err;
    }

    trace("Produced u16: 0x%04X (endianness: %d)", value, endianness);
    return PTK_OK;
}

ptk_err ptk_codec_produce_u32(ptk_buf_t *buf, u32 value, ptk_codec_endianness_t endianness) {
    if (!buf || !buf->data) {
        return PTK_ERR_NULL_PTR;
    }

    u32 encoded_value = apply_u32_endianness(value, endianness);
    u8 *bytes = (u8 *)&encoded_value;

    for (int i = 0; i < 4; i++) {
        ptk_err err = ptk_codec_produce_u8(buf, bytes[i]);
        if (err != PTK_OK) return err;
    }

    trace("Produced u32: 0x%08X (endianness: %d)", value, endianness);
    return PTK_OK;
}

ptk_err ptk_codec_produce_u64(ptk_buf_t *buf, u64 value, ptk_codec_endianness_t endianness) {
    if (!buf || !buf->data) {
        return PTK_ERR_NULL_PTR;
    }

    u64 encoded_value = apply_u64_endianness(value, endianness);
    u8 *bytes = (u8 *)&encoded_value;

    for (int i = 0; i < 8; i++) {
        ptk_err err = ptk_codec_produce_u8(buf, bytes[i]);
        if (err != PTK_OK) return err;
    }

    trace("Produced u64: 0x%016llX (endianness: %d)", (unsigned long long)value, endianness);
    return PTK_OK;
}

//=============================================================================
// BUFFER DECODING FUNCTIONS
//=============================================================================

ptk_err ptk_codec_consume_u8(ptk_buf_t *buf, u8 *value, bool peek) {
    if (!buf || !buf->data || !value) {
        return PTK_ERR_NULL_PTR;
    }

    // Check if we have data to read
    size_t available_data;
    ptk_err err = ptk_buf_len(&available_data, buf);
    if (err != PTK_OK) return err;
    
    if (available_data < 1) {
        error("Buffer underrun: need 1 byte, have %zu", available_data);
        return PTK_ERR_BUFFER_TOO_SMALL;
    }

    *value = buf->data->elements[buf->start];
    
    if (!peek) {
        buf->start++;
    }
    
    trace("Consumed u8: 0x%02X (peek: %s)", *value, peek ? "true" : "false");
    return PTK_OK;
}

ptk_err ptk_codec_consume_u16(ptk_buf_t *buf, u16 *value, ptk_codec_endianness_t endianness, bool peek) {
    if (!buf || !buf->data || !value) {
        return PTK_ERR_NULL_PTR;
    }

    // Check if we have enough data
    size_t available_data;
    ptk_err err = ptk_buf_len(&available_data, buf);
    if (err != PTK_OK) return err;
    
    if (available_data < 2) {
        error("Buffer underrun: need 2 bytes, have %zu", available_data);
        return PTK_ERR_BUFFER_TOO_SMALL;
    }

    u8 bytes[2];
    size_t original_start = buf->start;
    
    for (int i = 0; i < 2; i++) {
        err = ptk_codec_consume_u8(buf, &bytes[i], false);
        if (err != PTK_OK) {
            buf->start = original_start;  // Restore position on error
            return err;
        }
    }

    u16 raw_value = *(u16 *)bytes;
    *value = apply_u16_endianness(raw_value, endianness);

    if (peek) {
        buf->start = original_start;  // Restore position for peek
    }

    trace("Consumed u16: 0x%04X (endianness: %d, peek: %s)", *value, endianness, peek ? "true" : "false");
    return PTK_OK;
}

ptk_err ptk_codec_consume_u32(ptk_buf_t *buf, u32 *value, ptk_codec_endianness_t endianness, bool peek) {
    if (!buf || !buf->data || !value) {
        return PTK_ERR_NULL_PTR;
    }

    // Check if we have enough data
    size_t available_data;
    ptk_err err = ptk_buf_len(&available_data, buf);
    if (err != PTK_OK) return err;
    
    if (available_data < 4) {
        error("Buffer underrun: need 4 bytes, have %zu", available_data);
        return PTK_ERR_BUFFER_TOO_SMALL;
    }

    u8 bytes[4];
    size_t original_start = buf->start;
    
    for (int i = 0; i < 4; i++) {
        err = ptk_codec_consume_u8(buf, &bytes[i], false);
        if (err != PTK_OK) {
            buf->start = original_start;  // Restore position on error
            return err;
        }
    }

    u32 raw_value = *(u32 *)bytes;
    *value = apply_u32_endianness(raw_value, endianness);

    if (peek) {
        buf->start = original_start;  // Restore position for peek
    }

    trace("Consumed u32: 0x%08X (endianness: %d, peek: %s)", *value, endianness, peek ? "true" : "false");
    return PTK_OK;
}

ptk_err ptk_codec_consume_u64(ptk_buf_t *buf, u64 *value, ptk_codec_endianness_t endianness, bool peek) {
    if (!buf || !buf->data || !value) {
        return PTK_ERR_NULL_PTR;
    }

    // Check if we have enough data
    size_t available_data;
    ptk_err err = ptk_buf_len(&available_data, buf);
    if (err != PTK_OK) return err;
    
    if (available_data < 8) {
        error("Buffer underrun: need 8 bytes, have %zu", available_data);
        return PTK_ERR_BUFFER_TOO_SMALL;
    }

    u8 bytes[8];
    size_t original_start = buf->start;
    
    for (int i = 0; i < 8; i++) {
        err = ptk_codec_consume_u8(buf, &bytes[i], false);
        if (err != PTK_OK) {
            buf->start = original_start;  // Restore position on error
            return err;
        }
    }

    u64 raw_value = *(u64 *)bytes;
    *value = apply_u64_endianness(raw_value, endianness);

    if (peek) {
        buf->start = original_start;  // Restore position for peek
    }

    trace("Consumed u64: 0x%016llX (endianness: %d, peek: %s)", (unsigned long long)*value, endianness, peek ? "true" : "false");
    return PTK_OK;
}

//=============================================================================
// BYTE ARRAY ENCODING/DECODING
//=============================================================================

ptk_err ptk_codec_encode_u8_to_array(u8_array_t *data, size_t offset, u8 value) {
    if (!data) {
        return PTK_ERR_NULL_PTR;
    }

    if (offset >= data->len) {
        error("Array offset %zu out of bounds (len=%zu)", offset, data->len);
        return PTK_ERR_OUT_OF_BOUNDS;
    }

    data->elements[offset] = value;
    trace("Encoded u8 to array[%zu]: 0x%02X", offset, value);
    return PTK_OK;
}

ptk_err ptk_codec_encode_u16_to_array(u8_array_t *data, size_t offset, u16 value, ptk_codec_endianness_t endianness) {
    if (!data) {
        return PTK_ERR_NULL_PTR;
    }

    if (offset + 2 > data->len) {
        error("Array offset %zu+2 out of bounds (len=%zu)", offset, data->len);
        return PTK_ERR_OUT_OF_BOUNDS;
    }

    u16 encoded_value = apply_u16_endianness(value, endianness);
    u8 *bytes = (u8 *)&encoded_value;

    for (int i = 0; i < 2; i++) {
        data->elements[offset + i] = bytes[i];
    }

    trace("Encoded u16 to array[%zu]: 0x%04X (endianness: %d)", offset, value, endianness);
    return PTK_OK;
}

ptk_err ptk_codec_encode_u32_to_array(u8_array_t *data, size_t offset, u32 value, ptk_codec_endianness_t endianness) {
    if (!data) {
        return PTK_ERR_NULL_PTR;
    }

    if (offset + 4 > data->len) {
        error("Array offset %zu+4 out of bounds (len=%zu)", offset, data->len);
        return PTK_ERR_OUT_OF_BOUNDS;
    }

    u32 encoded_value = apply_u32_endianness(value, endianness);
    u8 *bytes = (u8 *)&encoded_value;

    for (int i = 0; i < 4; i++) {
        data->elements[offset + i] = bytes[i];
    }

    trace("Encoded u32 to array[%zu]: 0x%08X (endianness: %d)", offset, value, endianness);
    return PTK_OK;
}

ptk_err ptk_codec_encode_u64_to_array(u8_array_t *data, size_t offset, u64 value, ptk_codec_endianness_t endianness) {
    if (!data) {
        return PTK_ERR_NULL_PTR;
    }

    if (offset + 8 > data->len) {
        error("Array offset %zu+8 out of bounds (len=%zu)", offset, data->len);
        return PTK_ERR_OUT_OF_BOUNDS;
    }

    u64 encoded_value = apply_u64_endianness(value, endianness);
    u8 *bytes = (u8 *)&encoded_value;

    for (int i = 0; i < 8; i++) {
        data->elements[offset + i] = bytes[i];
    }

    trace("Encoded u64 to array[%zu]: 0x%016llX (endianness: %d)", offset, (unsigned long long)value, endianness);
    return PTK_OK;
}

ptk_err ptk_codec_decode_u8_from_array(const u8_array_t *data, size_t offset, u8 *value) {
    if (!data || !value) {
        return PTK_ERR_NULL_PTR;
    }

    if (offset >= data->len) {
        error("Array offset %zu out of bounds (len=%zu)", offset, data->len);
        return PTK_ERR_OUT_OF_BOUNDS;
    }

    *value = data->elements[offset];
    trace("Decoded u8 from array[%zu]: 0x%02X", offset, *value);
    return PTK_OK;
}

ptk_err ptk_codec_decode_u16_from_array(const u8_array_t *data, size_t offset, u16 *value, ptk_codec_endianness_t endianness) {
    if (!data || !value) {
        return PTK_ERR_NULL_PTR;
    }

    if (offset + 2 > data->len) {
        error("Array offset %zu+2 out of bounds (len=%zu)", offset, data->len);
        return PTK_ERR_OUT_OF_BOUNDS;
    }

    u8 bytes[2];
    for (int i = 0; i < 2; i++) {
        bytes[i] = data->elements[offset + i];
    }

    u16 raw_value = *(u16 *)bytes;
    *value = apply_u16_endianness(raw_value, endianness);

    trace("Decoded u16 from array[%zu]: 0x%04X (endianness: %d)", offset, *value, endianness);
    return PTK_OK;
}

ptk_err ptk_codec_decode_u32_from_array(const u8_array_t *data, size_t offset, u32 *value, ptk_codec_endianness_t endianness) {
    if (!data || !value) {
        return PTK_ERR_NULL_PTR;
    }

    if (offset + 4 > data->len) {
        error("Array offset %zu+4 out of bounds (len=%zu)", offset, data->len);
        return PTK_ERR_OUT_OF_BOUNDS;
    }

    u8 bytes[4];
    for (int i = 0; i < 4; i++) {
        bytes[i] = data->elements[offset + i];
    }

    u32 raw_value = *(u32 *)bytes;
    *value = apply_u32_endianness(raw_value, endianness);

    trace("Decoded u32 from array[%zu]: 0x%08X (endianness: %d)", offset, *value, endianness);
    return PTK_OK;
}

ptk_err ptk_codec_decode_u64_from_array(const u8_array_t *data, size_t offset, u64 *value, ptk_codec_endianness_t endianness) {
    if (!data || !value) {
        return PTK_ERR_NULL_PTR;
    }

    if (offset + 8 > data->len) {
        error("Array offset %zu+8 out of bounds (len=%zu)", offset, data->len);
        return PTK_ERR_OUT_OF_BOUNDS;
    }

    u8 bytes[8];
    for (int i = 0; i < 8; i++) {
        bytes[i] = data->elements[offset + i];
    }

    u64 raw_value = *(u64 *)bytes;
    *value = apply_u64_endianness(raw_value, endianness);

    trace("Decoded u64 from array[%zu]: 0x%016llX (endianness: %d)", offset, (unsigned long long)*value, endianness);
    return PTK_OK;
}

//=============================================================================
// BYTE ORDER MAP UTILITIES
//=============================================================================

ptk_err ptk_codec_apply_byte_order_map(u8_array_t *dest, size_t dest_offset,
                                      const u8 *src, size_t src_size,
                                      const size_t *byte_order_map) {
    if (!dest || !src || !byte_order_map) {
        return PTK_ERR_NULL_PTR;
    }

    if (dest_offset + src_size > dest->len) {
        error("Destination offset %zu+%zu out of bounds (len=%zu)", dest_offset, src_size, dest->len);
        return PTK_ERR_OUT_OF_BOUNDS;
    }

    for (size_t i = 0; i < src_size; i++) {
        size_t src_index = byte_order_map[i];
        if (src_index >= src_size) {
            error("Byte order map[%zu]=%zu out of bounds (src_size=%zu)", i, src_index, src_size);
            return PTK_ERR_OUT_OF_BOUNDS;
        }
        dest->elements[dest_offset + i] = src[src_index];
    }

    trace("Applied byte order map: %zu bytes", src_size);
    return PTK_OK;
}

ptk_err ptk_codec_reverse_byte_order_map(u8 *dest, size_t dest_size,
                                        const u8_array_t *src, size_t src_offset,
                                        const size_t *byte_order_map) {
    if (!dest || !src || !byte_order_map) {
        return PTK_ERR_NULL_PTR;
    }

    if (src_offset + dest_size > src->len) {
        error("Source offset %zu+%zu out of bounds (len=%zu)", src_offset, dest_size, src->len);
        return PTK_ERR_OUT_OF_BOUNDS;
    }

    for (size_t i = 0; i < dest_size; i++) {
        size_t dest_index = byte_order_map[i];
        if (dest_index >= dest_size) {
            error("Byte order map[%zu]=%zu out of bounds (dest_size=%zu)", i, dest_index, dest_size);
            return PTK_ERR_OUT_OF_BOUNDS;
        }
        dest[dest_index] = src->elements[src_offset + i];
    }

    trace("Reversed byte order map: %zu bytes", dest_size);
    return PTK_OK;
}

//=============================================================================
// VALIDATION UTILITIES
//=============================================================================

ptk_err ptk_codec_validate_array_bounds(const u8_array_t *data, size_t offset, size_t required_size) {
    if (!data) {
        return PTK_ERR_NULL_PTR;
    }

    if (offset + required_size > data->len) {
        error("Array bounds check failed: offset %zu + size %zu > len %zu", offset, required_size, data->len);
        return PTK_ERR_OUT_OF_BOUNDS;
    }

    return PTK_OK;
}

ptk_err ptk_codec_validate_buffer_bounds(const ptk_buf_t *buf, size_t required_size) {
    if (!buf || !buf->data) {
        return PTK_ERR_NULL_PTR;
    }

    size_t available;
    ptk_err err = ptk_buf_len((size_t *)&available, (ptk_buf_t *)buf);
    if (err != PTK_OK) return err;

    if (available < required_size) {
        error("Buffer bounds check failed: need %zu bytes, have %zu", required_size, available);
        return PTK_ERR_BUFFER_TOO_SMALL;
    }

    return PTK_OK;
}