#include "ptk_buf.h"
#include <stdlib.h>
#include <string.h>

//=============================================================================
// BUFFER OPERATIONS
//=============================================================================

ptk_err ptk_buf_make(ptk_buf_t *buf, uint8_t *data, size_t size) {
    if(!buf || !data) { return PTK_ERR_NULL_PTR; }

    buf->data = data;
    buf->capacity = size;
    buf->start = 0;
    buf->end = 0;

    return PTK_OK;
}

ptk_err ptk_buf_len(size_t *len, ptk_buf_t *buf) {
    if(!len || !buf) { return PTK_ERR_NULL_PTR; }

    *len = buf->end - buf->start;
    return PTK_OK;
}

ptk_err ptk_buf_cap(size_t *cap, ptk_buf_t *buf) {
    if(!cap || !buf) { return PTK_ERR_NULL_PTR; }

    *cap = buf->capacity;
    return PTK_OK;
}

ptk_err ptk_buf_get_start(size_t *start, ptk_buf_t *buf) {
    if(!start || !buf) { return PTK_ERR_NULL_PTR; }

    *start = buf->start;
    return PTK_OK;
}

ptk_err ptk_buf_get_start_ptr(uint8_t **ptr, ptk_buf_t *buf) {
    if(!ptr || !buf) { return PTK_ERR_NULL_PTR; }

    *ptr = buf->data + buf->start;
    return PTK_OK;
}

ptk_err ptk_buf_set_start(ptk_buf_t *buf, size_t start) {
    if(!buf) { return PTK_ERR_NULL_PTR; }

    if(start > buf->end) { return PTK_ERR_OUT_OF_BOUNDS; }

    buf->start = start;
    return PTK_OK;
}

ptk_err ptk_buf_get_end(size_t *end, ptk_buf_t *buf) {
    if(!end || !buf) { return PTK_ERR_NULL_PTR; }

    *end = buf->end;
    return PTK_OK;
}

ptk_err ptk_buf_get_end_ptr(uint8_t **ptr, ptk_buf_t *buf) {
    if(!ptr || !buf) { return PTK_ERR_NULL_PTR; }

    *ptr = buf->data + buf->end;
    return PTK_OK;
}

ptk_err ptk_buf_set_end(ptk_buf_t *buf, size_t end) {
    if(!buf) { return PTK_ERR_NULL_PTR; }

    if(end < buf->start || end > buf->capacity) { return PTK_ERR_OUT_OF_BOUNDS; }

    buf->end = end;
    return PTK_OK;
}

ptk_err ptk_buf_get_remaining(size_t *remaining, ptk_buf_t *buf) {
    if(!remaining || !buf) { return PTK_ERR_NULL_PTR; }

    *remaining = buf->capacity - buf->end;
    return PTK_OK;
}

ptk_err ptk_buf_move_to(ptk_buf_t *buf, size_t new_start) {
    if(!buf) { return PTK_ERR_NULL_PTR; }

    size_t data_len = buf->end - buf->start;

    // Check if new position would fit the data
    if(new_start + data_len > buf->capacity) { return PTK_ERR_OUT_OF_BOUNDS; }

    // Move data if there's any and positions are different
    if(data_len > 0 && new_start != buf->start) { memmove(buf->data + new_start, buf->data + buf->start, data_len); }

    buf->start = new_start;
    buf->end = new_start + data_len;

    return PTK_OK;
}

//=============================================================================
// ENDIAN-SPECIFIC HELPERS
//=============================================================================

static uint16_t swap_u16(uint16_t value) { return ((value & 0xFF) << 8) | ((value >> 8) & 0xFF); }

static uint32_t swap_u32(uint32_t value) {
    return ((value & 0xFF) << 24) | (((value >> 8) & 0xFF) << 16) | (((value >> 16) & 0xFF) << 8) | ((value >> 24) & 0xFF);
}

static uint64_t swap_u64(uint64_t value) {
    return ((value & 0xFF) << 56) | (((value >> 8) & 0xFF) << 48) | (((value >> 16) & 0xFF) << 40)
           | (((value >> 24) & 0xFF) << 32) | (((value >> 32) & 0xFF) << 24) | (((value >> 40) & 0xFF) << 16)
           | (((value >> 48) & 0xFF) << 8) | ((value >> 56) & 0xFF);
}

// Producer functions
ptk_err ptk_buf_produce_u8(ptk_buf_t *buf, uint8_t value) {
    if(!buf) { return PTK_ERR_NULL_PTR; }

    if(buf->end >= buf->capacity) { return PTK_ERR_BUFFER_TOO_SMALL; }

    buf->data[buf->end] = value;
    buf->end++;

    return PTK_OK;
}

ptk_err ptk_buf_produce_u16(ptk_buf_t *buf, uint16_t value, ptk_buf_endianness endianness) {
    if(!buf) { return PTK_ERR_NULL_PTR; }

    if(buf->end + 2 > buf->capacity) { return PTK_ERR_BUFFER_TOO_SMALL; }

    switch(endianness) {
        case PTK_BUF_BIG_ENDIAN:
        case PTK_BUF_LITTLE_ENDIAN_BYTE_SWAP:
            buf->data[buf->end] = (value >> 8) & 0xFF;
            buf->data[buf->end + 1] = value & 0xFF;
            break;
        case PTK_BUF_LITTLE_ENDIAN:
        case PTK_BUF_BIG_ENDIAN_BYTE_SWAP:
            buf->data[buf->end] = value & 0xFF;
            buf->data[buf->end + 1] = (value >> 8) & 0xFF;
            break;
    }

    buf->end += 2;
    return PTK_OK;
}

ptk_err ptk_buf_produce_u32(ptk_buf_t *buf, uint32_t value, ptk_buf_endianness endianness) {
    if(!buf) { return PTK_ERR_NULL_PTR; }

    if(buf->end + 4 > buf->capacity) { return PTK_ERR_BUFFER_TOO_SMALL; }

    switch(endianness) {
        case PTK_BUF_BIG_ENDIAN:
        case PTK_BUF_LITTLE_ENDIAN_BYTE_SWAP:
            buf->data[buf->end] = (value >> 24) & 0xFF;
            buf->data[buf->end + 1] = (value >> 16) & 0xFF;
            buf->data[buf->end + 2] = (value >> 8) & 0xFF;
            buf->data[buf->end + 3] = value & 0xFF;
            break;
        case PTK_BUF_LITTLE_ENDIAN:
        case PTK_BUF_BIG_ENDIAN_BYTE_SWAP:
            buf->data[buf->end] = value & 0xFF;
            buf->data[buf->end + 1] = (value >> 8) & 0xFF;
            buf->data[buf->end + 2] = (value >> 16) & 0xFF;
            buf->data[buf->end + 3] = (value >> 24) & 0xFF;
            break;
    }

    buf->end += 4;
    return PTK_OK;
}

ptk_err ptk_buf_produce_u64(ptk_buf_t *buf, uint64_t value, ptk_buf_endianness endianness) {
    if(!buf) { return PTK_ERR_NULL_PTR; }

    if(buf->end + 8 > buf->capacity) { return PTK_ERR_BUFFER_TOO_SMALL; }

    switch(endianness) {
        case PTK_BUF_BIG_ENDIAN:
        case PTK_BUF_LITTLE_ENDIAN_BYTE_SWAP:
            for(int i = 0; i < 8; i++) { buf->data[buf->end + i] = (value >> (56 - i * 8)) & 0xFF; }
            break;
        case PTK_BUF_LITTLE_ENDIAN:
        case PTK_BUF_BIG_ENDIAN_BYTE_SWAP:
            for(int i = 0; i < 8; i++) { buf->data[buf->end + i] = (value >> (i * 8)) & 0xFF; }
            break;
    }

    buf->end += 8;
    return PTK_OK;
}

// Consumer functions
ptk_err ptk_buf_consume_u8(ptk_buf_t *buf, uint8_t *value, bool peek) {
    if(!buf || !value) { return PTK_ERR_NULL_PTR; }

    if(buf->start >= buf->end) { return PTK_ERR_OUT_OF_BOUNDS; }

    *value = buf->data[buf->start];

    if(!peek) { buf->start++; }

    return PTK_OK;
}

ptk_err ptk_buf_consume_u16(ptk_buf_t *buf, uint16_t *value, ptk_buf_endianness endianness, bool peek) {
    if(!buf || !value) { return PTK_ERR_NULL_PTR; }

    if(buf->start + 2 > buf->end) { return PTK_ERR_OUT_OF_BOUNDS; }

    switch(endianness) {
        case PTK_BUF_BIG_ENDIAN:
        case PTK_BUF_LITTLE_ENDIAN_BYTE_SWAP:
            *value = ((uint16_t)buf->data[buf->start] << 8) | (uint16_t)buf->data[buf->start + 1];
            break;
        case PTK_BUF_LITTLE_ENDIAN:
        case PTK_BUF_BIG_ENDIAN_BYTE_SWAP:
            *value = (uint16_t)buf->data[buf->start] | ((uint16_t)buf->data[buf->start + 1] << 8);
            break;
    }

    if(!peek) { buf->start += 2; }

    return PTK_OK;
}

ptk_err ptk_buf_consume_u32(ptk_buf_t *buf, uint32_t *value, ptk_buf_endianness endianness, bool peek) {
    if(!buf || !value) { return PTK_ERR_NULL_PTR; }

    if(buf->start + 4 > buf->end) { return PTK_ERR_OUT_OF_BOUNDS; }

    switch(endianness) {
        case PTK_BUF_BIG_ENDIAN:
        case PTK_BUF_LITTLE_ENDIAN_BYTE_SWAP:
            *value = ((uint32_t)buf->data[buf->start] << 24) | ((uint32_t)buf->data[buf->start + 1] << 16)
                     | ((uint32_t)buf->data[buf->start + 2] << 8) | (uint32_t)buf->data[buf->start + 3];
            break;
        case PTK_BUF_LITTLE_ENDIAN:
        case PTK_BUF_BIG_ENDIAN_BYTE_SWAP:
            *value = (uint32_t)buf->data[buf->start] | ((uint32_t)buf->data[buf->start + 1] << 8)
                     | ((uint32_t)buf->data[buf->start + 2] << 16) | ((uint32_t)buf->data[buf->start + 3] << 24);
            break;
    }

    if(!peek) { buf->start += 4; }

    return PTK_OK;
}

ptk_err ptk_buf_consume_u64(ptk_buf_t *buf, uint64_t *value, ptk_buf_endianness endianness, bool peek) {
    if(!buf || !value) { return PTK_ERR_NULL_PTR; }

    if(buf->start + 8 > buf->end) { return PTK_ERR_OUT_OF_BOUNDS; }

    *value = 0;

    switch(endianness) {
        case PTK_BUF_BIG_ENDIAN:
        case PTK_BUF_LITTLE_ENDIAN_BYTE_SWAP:
            for(int i = 0; i < 8; i++) { *value |= ((uint64_t)buf->data[buf->start + i]) << (56 - i * 8); }
            break;
        case PTK_BUF_LITTLE_ENDIAN:
        case PTK_BUF_BIG_ENDIAN_BYTE_SWAP:
            for(int i = 0; i < 8; i++) { *value |= ((uint64_t)buf->data[buf->start + i]) << (i * 8); }
            break;
    }

    if(!peek) { buf->start += 8; }

    return PTK_OK;
}