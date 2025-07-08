#include "ptk_codec.h"
#include "ptk_buf.h"
#include "ptk_log.h"
#include <arpa/inet.h>  // For htons, htonl, etc.
#include <string.h>

//=============================================================================
// INTERNAL HELPERS
//=============================================================================

static inline u16 codec_swap_u16(u16 value) { return ((value & 0xFF) << 8) | ((value >> 8) & 0xFF); }

static inline u32 codec_swap_u32(u32 value) {
    return ((value & 0xFF) << 24) | (((value >> 8) & 0xFF) << 16) | (((value >> 16) & 0xFF) << 8) | ((value >> 24) & 0xFF);
}

static inline u64 codec_swap_u64(u64 value) {
    return ((value & 0xFF) << 56) | (((value >> 8) & 0xFF) << 48) | (((value >> 16) & 0xFF) << 40)
           | (((value >> 24) & 0xFF) << 32) | (((value >> 32) & 0xFF) << 24) | (((value >> 40) & 0xFF) << 16)
           | (((value >> 48) & 0xFF) << 8) | ((value >> 56) & 0xFF);
}

/**
 * @brief apply endian corrections to the passed in little endian value.
 *
 * @param correct_val where to store the corrected value
 * @param value_le
 * @param endianness
 * @return u16
 */
static ptk_err apply_u16_endianness(u16 *correct_val, u16 value_le, ptk_codec_endianness_t endianness) {
    if(!correct_val) { return PTK_ERR_NULL_PTR; }

    if(endianness == PTK_CODEC_BIG_ENDIAN) {
        *correct_val = 0;
        *correct_val |= ((u16)(value_le >> 0) << 8);
        *correct_val |= ((u16)(value_le >> 8) << 0);
    } else if(endianness == PTK_CODEC_LITTLE_ENDIAN) {
        *correct_val = value_le;
    } else {
        return PTK_ERR_UNSUPPORTED;
    }

    return PTK_OK;
}

static ptk_err apply_u32_endianness(u32 *correct_val, u32 value_le, ptk_codec_endianness_t endianness) {
    if(!correct_val) { return PTK_ERR_NULL_PTR; }

    if(endianness == PTK_CODEC_BIG_ENDIAN) {
        *correct_val = 0;
        *correct_val |= (((value_le >> 0) & 0xFF) << 24);
        *correct_val |= (((value_le >> 8) & 0xFF) << 16);
        *correct_val |= (((value_le >> 16) & 0xFF) << 8);
        *correct_val |= (((value_le >> 24) & 0xFF) << 0);
    } else if(endianness == PTK_CODEC_BIG_ENDIAN_BYTE_SWAP) {
        *correct_val = 0;
        *correct_val |= (((value_le >> 0) & 0xFF) << 16);
        *correct_val |= (((value_le >> 8) & 0xFF) << 24);
        *correct_val |= (((value_le >> 16) & 0xFF) << 0);
        *correct_val |= (((value_le >> 24) & 0xFF) << 8);
    } else if(endianness == PTK_CODEC_LITTLE_ENDIAN) {
        *correct_val = value_le;
    } else if(endianness == PTK_CODEC_LITTLE_ENDIAN_BYTE_SWAP) {
        *correct_val = 0;
        *correct_val |= (((value_le >> 0) & 0xFF) << 8);
        *correct_val |= (((value_le >> 8) & 0xFF) << 0);
        *correct_val |= (((value_le >> 16) & 0xFF) << 24);
        *correct_val |= (((value_le >> 24) & 0xFF) << 16);
    } else {
        return PTK_ERR_UNSUPPORTED;
    }

    return PTK_OK;
}

static ptk_err apply_u64_endianness(u64 *correct_val, u64 value_le, ptk_codec_endianness_t endianness) {
    if(!correct_val) { return PTK_ERR_NULL_PTR; }

    if(endianness == PTK_CODEC_BIG_ENDIAN) {
        *correct_val = 0;
        *correct_val |= (((value_le >> 0) & 0xFF) << 56);
        *correct_val |= (((value_le >> 8) & 0xFF) << 48);
        *correct_val |= (((value_le >> 16) & 0xFF) << 40);
        *correct_val |= (((value_le >> 24) & 0xFF) << 32);
        *correct_val |= (((value_le >> 32) & 0xFF) << 24);
        *correct_val |= (((value_le >> 40) & 0xFF) << 16);
        *correct_val |= (((value_le >> 48) & 0xFF) << 8);
        *correct_val |= (((value_le >> 56) & 0xFF) << 0);
    } else if(endianness == PTK_CODEC_BIG_ENDIAN_BYTE_SWAP) {
        *correct_val = 0;
        *correct_val |= (((value_le >> 0) & 0xFF) << 48);
        *correct_val |= (((value_le >> 8) & 0xFF) << 56);
        *correct_val |= (((value_le >> 16) & 0xFF) << 32);
        *correct_val |= (((value_le >> 24) & 0xFF) << 40);
        *correct_val |= (((value_le >> 32) & 0xFF) << 16);
        *correct_val |= (((value_le >> 40) & 0xFF) << 24);
        *correct_val |= (((value_le >> 48) & 0xFF) << 8);
        *correct_val |= (((value_le >> 56) & 0xFF) << 0);
    } else if(endianness == PTK_CODEC_LITTLE_ENDIAN) {
        *correct_val = value_le;
    } else if(endianness == PTK_CODEC_LITTLE_ENDIAN_BYTE_SWAP) {
        *correct_val = 0;
        *correct_val |= (((value_le >> 0) & 0xFF) << 8);
        *correct_val |= (((value_le >> 8) & 0xFF) << 0);
        *correct_val |= (((value_le >> 16) & 0xFF) << 24);
        *correct_val |= (((value_le >> 24) & 0xFF) << 16);
        *correct_val |= (((value_le >> 32) & 0xFF) << 40);
        *correct_val |= (((value_le >> 40) & 0xFF) << 32);
        *correct_val |= (((value_le >> 48) & 0xFF) << 56);
        *correct_val |= (((value_le >> 56) & 0xFF) << 48);
    } else {
        return PTK_ERR_UNSUPPORTED;
    }

    return PTK_OK;
}

//=============================================================================
// BUFFER ENCODING FUNCTIONS
//=============================================================================

ptk_err ptk_codec_produce_u8(ptk_buf_t *buf, u8 value) { return ptk_buf_produce_u8(buf, value); }

ptk_err ptk_codec_produce_u16(ptk_buf_t *buf, u16 value, ptk_codec_endianness_t endianness) {
    ptk_err err = PTK_OK;
    u16 correct_val = 0;

    if(!buf) { return PTK_ERR_NULL_PTR; }

    err = apply_u16_endianness(&correct_val, value, endianness);
    if(err != PTK_OK) { return err; }

    err = ptk_buf_produce_u8(buf, (u8)(value & 0xFF));
    if(err != PTK_OK) { return err; }

    err = ptk_buf_produce_u8(buf, (u8)(value >> 8));
    if(err != PTK_OK) { return err; }

    return PTK_OK;
}

ptk_err ptk_codec_produce_u32(ptk_buf_t *buf, u32 value, ptk_codec_endianness_t endianness) {
    ptk_err err = PTK_OK;
    u32 correct_val = 0;

    if(!buf) { return PTK_ERR_NULL_PTR; }

    err = apply_u32_endianness(&correct_val, value, endianness);
    if(err != PTK_OK) { return err; }

    err = ptk_buf_produce_u8(buf, (u8)(value & 0xFF));
    if(err != PTK_OK) { return err; }

    err = ptk_buf_produce_u8(buf, (u8)(value >> 8));
    if(err != PTK_OK) { return err; }

    err = ptk_buf_produce_u8(buf, (u8)(value >> 16));
    if(err != PTK_OK) { return err; }

    err = ptk_buf_produce_u8(buf, (u8)(value >> 24));
    if(err != PTK_OK) { return err; }

    return PTK_OK;
}

ptk_err ptk_codec_produce_u64(ptk_buf_t *buf, u64 value, ptk_codec_endianness_t endianness) {
    ptk_err err = PTK_OK;
    u64 correct_val = 0;

    if(!buf) { return PTK_ERR_NULL_PTR; }

    err = apply_u64_endianness(&correct_val, value, endianness);
    if(err != PTK_OK) { return err; }

    err = ptk_buf_produce_u8(buf, (u8)(value & 0xFF));
    if(err != PTK_OK) { return err; }

    err = ptk_buf_produce_u8(buf, (u8)(value >> 8));
    if(err != PTK_OK) { return err; }

    err = ptk_buf_produce_u8(buf, (u8)(value >> 16));
    if(err != PTK_OK) { return err; }

    err = ptk_buf_produce_u8(buf, (u8)(value >> 24));
    if(err != PTK_OK) { return err; }

    err = ptk_buf_produce_u8(buf, (u8)(value >> 32));
    if(err != PTK_OK) { return err; }

    err = ptk_buf_produce_u8(buf, (u8)(value >> 40));
    if(err != PTK_OK) { return err; }

    err = ptk_buf_produce_u8(buf, (u8)(value >> 48));
    if(err != PTK_OK) { return err; }

    err = ptk_buf_produce_u8(buf, (u8)(value >> 56));
    if(err != PTK_OK) { return err; }

    return PTK_OK;
}


//=============================================================================
// BUFFER DECODING FUNCTIONS
//=============================================================================

ptk_err ptk_codec_consume_u8(u8 *value, ptk_buf_t *buf, bool peek) { return ptk_buf_consume_u8(value, buf, peek); }

ptk_err ptk_codec_consume_u16(u16 *value, ptk_codec_endianness_t endianness, ptk_buf_t *buf, bool peek) {
    ptk_err err = PTK_OK;
    size_t old_start_index = 0;
    u16 tmp_val = 0;
    u8 u8_val = 0;

    if(!buf || !value) { return PTK_ERR_NULL_PTR; }

    err = ptk_buf_get_start(&old_start_index, buf);
    if(err != PTK_OK) { return err; }

    *value = 0;

    err = ptk_buf_consume_u8(&u8_val, buf, false);
    if(err != PTK_OK) { return err; }
    tmp_val |= ((u16)u8_val);

    err = ptk_buf_consume_u8(&u8_val, buf, false);
    if(err != PTK_OK) { return err; }
    tmp_val |= ((u16)u8_val << 8);

    err = apply_u16_endianness(value, tmp_val, endianness);

    if(peek) { ptk_buf_set_start(buf, old_start_index); }

    return PTK_OK;
}

ptk_err ptk_codec_consume_u32(u32 *value, ptk_codec_endianness_t endianness, ptk_buf_t *buf, bool peek) {
    ptk_err err = PTK_OK;
    size_t old_start_index = 0;
    u32 tmp_val = 0;
    u8 u8_val = 0;

    if(!buf || !value) { return PTK_ERR_NULL_PTR; }

    err = ptk_buf_get_start(&old_start_index, buf);
    if(err != PTK_OK) { return err; }

    *value = 0;

    err = ptk_buf_consume_u8(&u8_val, buf, false);
    if(err != PTK_OK) { return err; }
    tmp_val |= ((u32)u8_val);

    err = ptk_buf_consume_u8(&u8_val, buf, false);
    if(err != PTK_OK) { return err; }
    tmp_val |= ((u32)u8_val << 8);

    err = ptk_buf_consume_u8(&u8_val, buf, false);
    if(err != PTK_OK) { return err; }
    tmp_val |= ((u32)u8_val << 16);

    err = ptk_buf_consume_u8(&u8_val, buf, false);
    if(err != PTK_OK) { return err; }
    tmp_val |= ((u32)u8_val << 24);

    err = apply_u32_endianness(value, tmp_val, endianness);

    if(peek) { ptk_buf_set_start(buf, old_start_index); }

    return PTK_OK;
}

ptk_err ptk_codec_consume_u64(u64 *value, ptk_codec_endianness_t endianness, ptk_buf_t *buf, bool peek) {
    ptk_err err = PTK_OK;
    size_t old_start_index = 0;
    u64 tmp_val = 0;
    u8 u8_val = 0;

    if(!buf || !value) { return PTK_ERR_NULL_PTR; }

    err = ptk_buf_get_start(&old_start_index, buf);
    if(err != PTK_OK) { return err; }

    *value = 0;

    err = ptk_buf_consume_u8(&u8_val, buf, false);
    if(err != PTK_OK) { return err; }
    tmp_val |= ((u64)u8_val);

    err = ptk_buf_consume_u8(&u8_val, buf, false);
    if(err != PTK_OK) { return err; }
    tmp_val |= ((u64)u8_val << 8);

    err = ptk_buf_consume_u8(&u8_val, buf, false);
    if(err != PTK_OK) { return err; }
    tmp_val |= ((u64)u8_val << 16);

    err = ptk_buf_consume_u8(&u8_val, buf, false);
    if(err != PTK_OK) { return err; }
    tmp_val |= ((u64)u8_val << 24);

    err = ptk_buf_consume_u8(&u8_val, buf, false);
    if(err != PTK_OK) { return err; }
    tmp_val |= ((u64)u8_val << 32);

    err = ptk_buf_consume_u8(&u8_val, buf, false);
    if(err != PTK_OK) { return err; }
    tmp_val |= ((u64)u8_val << 40);

    err = ptk_buf_consume_u8(&u8_val, buf, false);
    if(err != PTK_OK) { return err; }
    tmp_val |= ((u64)u8_val << 48);

    err = ptk_buf_consume_u8(&u8_val, buf, false);
    if(err != PTK_OK) { return err; }
    tmp_val |= ((u64)u8_val << 56);

    err = apply_u64_endianness(value, tmp_val, endianness);

    if(peek) { ptk_buf_set_start(buf, old_start_index); }

    return PTK_OK;
}


// //=============================================================================
// // BYTE ARRAY ENCODING/DECODING
// //=============================================================================

// ptk_err ptk_codec_encode_u8_to_array(u8_array_t *data, size_t offset, u8 value) {
//     if(!data) { return PTK_ERR_NULL_PTR; }

//     if(offset >= data->len) {
//         error("Array offset %zu out of bounds (len=%zu)", offset, data->len);
//         return PTK_ERR_OUT_OF_BOUNDS;
//     }

//     data->elements[offset] = value;
//     trace("Encoded u8 to array[%zu]: 0x%02X", offset, value);
//     return PTK_OK;
// }

// ptk_err ptk_codec_encode_u16_to_array(u8_array_t *data, size_t offset, u16 value, ptk_codec_endianness_t endianness) {
//     if(!data) { return PTK_ERR_NULL_PTR; }

//     if(offset + 2 > data->len) {
//         error("Array offset %zu+2 out of bounds (len=%zu)", offset, data->len);
//         return PTK_ERR_OUT_OF_BOUNDS;
//     }

//     u16 encoded_value = apply_u16_endianness(value, endianness);
//     u8 *bytes = (u8 *)&encoded_value;

//     for(int i = 0; i < 2; i++) { data->elements[offset + i] = bytes[i]; }

//     trace("Encoded u16 to array[%zu]: 0x%04X (endianness: %d)", offset, value, endianness);
//     return PTK_OK;
// }

// ptk_err ptk_codec_encode_u32_to_array(u8_array_t *data, size_t offset, u32 value, ptk_codec_endianness_t endianness) {
//     if(!data) { return PTK_ERR_NULL_PTR; }

//     if(offset + 4 > data->len) {
//         error("Array offset %zu+4 out of bounds (len=%zu)", offset, data->len);
//         return PTK_ERR_OUT_OF_BOUNDS;
//     }

//     u32 encoded_value = apply_u32_endianness(value, endianness);
//     u8 *bytes = (u8 *)&encoded_value;

//     for(int i = 0; i < 4; i++) { data->elements[offset + i] = bytes[i]; }

//     trace("Encoded u32 to array[%zu]: 0x%08X (endianness: %d)", offset, value, endianness);
//     return PTK_OK;
// }

// ptk_err ptk_codec_encode_u64_to_array(u8_array_t *data, size_t offset, u64 value, ptk_codec_endianness_t endianness) {
//     if(!data) { return PTK_ERR_NULL_PTR; }

//     if(offset + 8 > data->len) {
//         error("Array offset %zu+8 out of bounds (len=%zu)", offset, data->len);
//         return PTK_ERR_OUT_OF_BOUNDS;
//     }

//     u64 encoded_value = apply_u64_endianness(value, endianness);
//     u8 *bytes = (u8 *)&encoded_value;

//     for(int i = 0; i < 8; i++) { data->elements[offset + i] = bytes[i]; }

//     trace("Encoded u64 to array[%zu]: 0x%016llX (endianness: %d)", offset, (unsigned long long)value, endianness);
//     return PTK_OK;
// }

// ptk_err ptk_codec_decode_u8_from_array(const u8_array_t *data, size_t offset, u8 *value) {
//     if(!data || !value) { return PTK_ERR_NULL_PTR; }

//     if(offset >= data->len) {
//         error("Array offset %zu out of bounds (len=%zu)", offset, data->len);
//         return PTK_ERR_OUT_OF_BOUNDS;
//     }

//     *value = data->elements[offset];
//     trace("Decoded u8 from array[%zu]: 0x%02X", offset, *value);
//     return PTK_OK;
// }

// ptk_err ptk_codec_decode_u16_from_array(const u8_array_t *data, size_t offset, u16 *value, ptk_codec_endianness_t endianness) {
//     if(!data || !value) { return PTK_ERR_NULL_PTR; }

//     if(offset + 2 > data->len) {
//         error("Array offset %zu+2 out of bounds (len=%zu)", offset, data->len);
//         return PTK_ERR_OUT_OF_BOUNDS;
//     }

//     u8 bytes[2];
//     for(int i = 0; i < 2; i++) { bytes[i] = data->elements[offset + i]; }

//     u16 raw_value = *(u16 *)bytes;
//     *value = apply_u16_endianness(raw_value, endianness);

//     trace("Decoded u16 from array[%zu]: 0x%04X (endianness: %d)", offset, *value, endianness);
//     return PTK_OK;
// }

// ptk_err ptk_codec_decode_u32_from_array(const u8_array_t *data, size_t offset, u32 *value, ptk_codec_endianness_t endianness) {
//     if(!data || !value) { return PTK_ERR_NULL_PTR; }

//     if(offset + 4 > data->len) {
//         error("Array offset %zu+4 out of bounds (len=%zu)", offset, data->len);
//         return PTK_ERR_OUT_OF_BOUNDS;
//     }

//     u8 bytes[4];
//     for(int i = 0; i < 4; i++) { bytes[i] = data->elements[offset + i]; }

//     u32 raw_value = *(u32 *)bytes;
//     *value = apply_u32_endianness(raw_value, endianness);

//     trace("Decoded u32 from array[%zu]: 0x%08X (endianness: %d)", offset, *value, endianness);
//     return PTK_OK;
// }

// ptk_err ptk_codec_decode_u64_from_array(const u8_array_t *data, size_t offset, u64 *value, ptk_codec_endianness_t endianness) {
//     if(!data || !value) { return PTK_ERR_NULL_PTR; }

//     if(offset + 8 > data->len) {
//         error("Array offset %zu+8 out of bounds (len=%zu)", offset, data->len);
//         return PTK_ERR_OUT_OF_BOUNDS;
//     }

//     u8 bytes[8];
//     for(int i = 0; i < 8; i++) { bytes[i] = data->elements[offset + i]; }

//     u64 raw_value = *(u64 *)bytes;
//     *value = apply_u64_endianness(raw_value, endianness);

//     trace("Decoded u64 from array[%zu]: 0x%016llX (endianness: %d)", offset, (unsigned long long)*value, endianness);
//     return PTK_OK;
// }

// //=============================================================================
// // BYTE ORDER MAP UTILITIES
// //=============================================================================

// ptk_err ptk_codec_apply_byte_order_map(u8_array_t *dest, size_t dest_offset, const u8 *src, size_t src_size,
//                                        const size_t *byte_order_map) {
//     if(!dest || !src || !byte_order_map) { return PTK_ERR_NULL_PTR; }

//     if(dest_offset + src_size > dest->len) {
//         error("Destination offset %zu+%zu out of bounds (len=%zu)", dest_offset, src_size, dest->len);
//         return PTK_ERR_OUT_OF_BOUNDS;
//     }

//     for(size_t i = 0; i < src_size; i++) {
//         size_t src_index = byte_order_map[i];
//         if(src_index >= src_size) {
//             error("Byte order map[%zu]=%zu out of bounds (src_size=%zu)", i, src_index, src_size);
//             return PTK_ERR_OUT_OF_BOUNDS;
//         }
//         dest->elements[dest_offset + i] = src[src_index];
//     }

//     trace("Applied byte order map: %zu bytes", src_size);
//     return PTK_OK;
// }

// ptk_err ptk_codec_reverse_byte_order_map(u8 *dest, size_t dest_size, const u8_array_t *src, size_t src_offset,
//                                          const size_t *byte_order_map) {
//     if(!dest || !src || !byte_order_map) { return PTK_ERR_NULL_PTR; }

//     if(src_offset + dest_size > src->len) {
//         error("Source offset %zu+%zu out of bounds (len=%zu)", src_offset, dest_size, src->len);
//         return PTK_ERR_OUT_OF_BOUNDS;
//     }

//     for(size_t i = 0; i < dest_size; i++) {
//         size_t dest_index = byte_order_map[i];
//         if(dest_index >= dest_size) {
//             error("Byte order map[%zu]=%zu out of bounds (dest_size=%zu)", i, dest_index, dest_size);
//             return PTK_ERR_OUT_OF_BOUNDS;
//         }
//         dest[dest_index] = src->elements[src_offset + i];
//     }

//     trace("Reversed byte order map: %zu bytes", dest_size);
//     return PTK_OK;
// }

// //=============================================================================
// // VALIDATION UTILITIES
// //=============================================================================

// ptk_err ptk_codec_validate_array_bounds(const u8_array_t *data, size_t offset, size_t required_size) {
//     if(!data) { return PTK_ERR_NULL_PTR; }

//     if(offset + required_size > data->len) {
//         error("Array bounds check failed: offset %zu + size %zu > len %zu", offset, required_size, data->len);
//         return PTK_ERR_OUT_OF_BOUNDS;
//     }

//     return PTK_OK;
// }

// ptk_err ptk_codec_validate_buffer_bounds(const ptk_buf_t *buf, size_t required_size) {
//     if(!buf || !buf->data) { return PTK_ERR_NULL_PTR; }

//     size_t available;
//     ptk_err err = ptk_buf_len((size_t *)&available, (ptk_buf_t *)buf);
//     if(err != PTK_OK) { return err; }

//     if(available < required_size) {
//         error("Buffer bounds check failed: need %zu bytes, have %zu", required_size, available);
//         return PTK_ERR_BUFFER_TOO_SMALL;
//     }

//     return PTK_OK;
// }