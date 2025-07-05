#include "buf.h"
#include "log.h"
#include <stdarg.h>
#include <string.h>
#include <arpa/inet.h>

/*
 * Format string parsing state
 */
typedef struct {
    bool big_endian;      // true = big endian, false = little endian
    bool byte_swap;       // true = byte swap on 16-bit boundaries
    const char *fmt;      // current position in format string
} fmt_state_t;

/*
 * Parse format string and extract the next type specification
 */
static const char* parse_next_type(fmt_state_t *state, char *type, int *count) {
    trace("Parsing format string at position: %.10s", state->fmt);
    
    *count = 1;  // default count
    *type = 0;
    
    while (*state->fmt) {
        switch (*state->fmt) {
            case '>':
                state->big_endian = true;
                state->byte_swap = false;
                trace("Set big endian mode");
                break;
            case '<':
                state->big_endian = false;
                state->byte_swap = false;
                trace("Set little endian mode");
                break;
            case '$':
                state->byte_swap = true;
                trace("Set byte swap mode");
                break;
            case ' ':
            case '\t':
                // Skip whitespace
                break;
            case '.':
                *type = '.';
                state->fmt++;
                trace("Found skip byte marker");
                return state->fmt;
            case '0'...'9':
                // Parse count
                *count = 0;
                while (*state->fmt >= '0' && *state->fmt <= '9') {
                    *count = (*count * 10) + (*state->fmt - '0');
                    state->fmt++;
                }
                state->fmt--; // Back up one since we'll increment at the end
                trace("Parsed count: %d", *count);
                break;
            case 'u':
            case 'i':
            case 'f':
                // Found a type specifier
                *type = *state->fmt;
                state->fmt++;
                trace("Found type: %c", *type);
                return state->fmt;
            default:
                warn("Unknown format character: %c", *state->fmt);
                return NULL;
        }
        state->fmt++;
    }
    
    return state->fmt;
}

/*
 * Get the size and actual type from the type character and following digits
 */
static buf_err_t parse_type_size(fmt_state_t *state, char type_char, int *size, bool *is_signed, bool *is_float) {
    trace("Parsing type size for: %c", type_char);
    
    *is_signed = false;
    *is_float = false;
    *size = 1;
    
    if (type_char == 'i') {
        *is_signed = true;
    } else if (type_char == 'f') {
        *is_float = true;
    }
    
    // Parse the size digits
    if (*state->fmt >= '0' && *state->fmt <= '9') {
        int type_size = 0;
        while (*state->fmt >= '0' && *state->fmt <= '9') {
            type_size = (type_size * 10) + (*state->fmt - '0');
            state->fmt++;
        }
        
        if (type_size == 8) {
            *size = 1;
        } else if (type_size == 16) {
            *size = 2;
        } else if (type_size == 32) {
            *size = 4;
        } else if (type_size == 64) {
            *size = 8;
        } else {
            warn("Invalid type size: %d", type_size);
            return BUF_ERR_BAD_FORMAT;
        }
    }
    
    trace("Parsed type: size=%d, signed=%d, float=%d", *size, *is_signed, *is_float);
    return BUF_OK;
}

/*
 * Perform byte swapping on 16-bit boundaries
 */
static void byte_swap_16bit_boundaries(uint8_t *data, int size) {
    if (!data || size < 2) return;
    
    for (int i = 0; i < size; i += 2) {
        if (i + 1 < size) {
            uint8_t temp = data[i];
            data[i] = data[i + 1];
            data[i + 1] = temp;
        }
    }
}

/*
 * Convert endianness for multi-byte values
 */
static void convert_endianness(uint8_t *data, int size, bool to_big_endian, bool byte_swap) {
    if (!data || size <= 1) return;
    
    // First apply byte swapping if requested
    if (byte_swap) {
        byte_swap_16bit_boundaries(data, size);
        trace("Applied byte swap on 16-bit boundaries");
    }
    
    // Determine host endianness
    uint16_t test = 1;
    bool host_is_big_endian = (*(uint8_t*)&test == 0);
    
    // Convert if endianness doesn't match
    if (host_is_big_endian != to_big_endian) {
        for (int i = 0; i < size / 2; i++) {
            uint8_t temp = data[i];
            data[i] = data[size - 1 - i];
            data[size - 1 - i] = temp;
        }
        trace("Converted endianness: host_big=%d, target_big=%d", host_is_big_endian, to_big_endian);
    }
}

buf_err_t buf_decode(buf *src, bool peek, const char *fmt, ...) {
    trace("Starting buf_decode: peek=%d, fmt='%s'", peek, fmt);
    
    if (!src || !fmt) {
        error("buf_decode: NULL pointer passed");
        return BUF_ERR_NULL_PTR;
    }
    
    if (!src->data) {
        error("buf_decode: Buffer data is NULL");
        return BUF_ERR_NULL_PTR;
    }
    
    va_list args;
    va_start(args, fmt);
    
    fmt_state_t state = {
        .big_endian = true,  // Default to big endian
        .byte_swap = false,
        .fmt = fmt
    };
    
    size_t original_cursor = src->cursor;
    buf_err_t result = BUF_OK;
    
    while (*state.fmt && result == BUF_OK) {
        char type;
        int count;
        
        const char* next_pos = parse_next_type(&state, &type, &count);
        if (!next_pos) {
            result = BUF_ERR_BAD_FORMAT;
            break;
        }
        
        if (type == '.') {
            // Skip bytes
            if (src->cursor + count > src->len) {
                warn("Not enough data to skip %d bytes", count);
                result = BUF_ERR_OUT_OF_BOUNDS;
                break;
            }
            if (!peek) {
                src->cursor += count;
            }
            trace("Skipped %d bytes", count);
            continue;
        }
        
        if (type == 'B') {
            // Bit array type
            if (count <= 0) {
                warn("Invalid bit count for bit array: %d", count);
                result = BUF_ERR_BAD_FORMAT;
                break;
            }
            
            // Calculate bytes needed for bit count
            size_t bytes_needed = (count + 7) / 8;
            if (src->cursor + bytes_needed > src->len) {
                warn("Not enough data for bit array: need %zu bytes, have %zu", bytes_needed, src->len - src->cursor);
                result = BUF_ERR_OUT_OF_BOUNDS;
                break;
            }
            
            // Get the destination pointer
            bool *dest_bits = va_arg(args, bool*);
            if (!dest_bits) {
                error("NULL destination pointer for bit array");
                result = BUF_ERR_NULL_PTR;
                break;
            }
            
            // Unpack bits from bytes (LSB first)
            for (int bit = 0; bit < count; bit++) {
                int byte_idx = bit / 8;
                int bit_idx = bit % 8;
                uint8_t byte_val = src->data[src->cursor + byte_idx];
                dest_bits[bit] = (byte_val & (1 << bit_idx)) != 0;
            }
            
            if (!peek) {
                src->cursor += bytes_needed;
            }
            
            trace("Decoded bit array: %d bits (%zu bytes)", count, bytes_needed);
            
        } else if (type == 'u' || type == 'i' || type == 'f') {
            int size;
            bool is_signed, is_float;
            
            result = parse_type_size(&state, type, &size, &is_signed, &is_float);
            if (result != BUF_OK) break;
            
            // Calculate total bytes needed
            size_t total_bytes = size * count;
            if (src->cursor + total_bytes > src->len) {
                warn("Not enough data: need %zu bytes, have %zu", total_bytes, src->len - src->cursor);
                result = BUF_ERR_OUT_OF_BOUNDS;
                break;
            }
            
            // Get the destination pointer
            void *dest = va_arg(args, void*);
            if (!dest) {
                error("NULL destination pointer for type %c", type);
                result = BUF_ERR_NULL_PTR;
                break;
            }
            
            // Copy data
            memcpy(dest, src->data + src->cursor, total_bytes);
            
            // Apply endianness conversion if needed
            if (size > 1) {
                for (int i = 0; i < count; i++) {
                    convert_endianness((uint8_t*)dest + (i * size), size, state.big_endian, state.byte_swap);
                }
            }
            
            if (!peek) {
                src->cursor += total_bytes;
            }
            
            trace("Decoded %d items of type %c%d (total %zu bytes)", count, type, size * 8, total_bytes);
        }
    }
    
    va_end(args);
    
    if (result != BUF_OK && peek) {
        // Restore cursor position on error during peek
        src->cursor = original_cursor;
    }
    
    if (result == BUF_OK) {
        info("buf_decode completed successfully, cursor at %zu", src->cursor);
    } else {
        error("buf_decode failed with error code %d", result);
    }
    
    return result;
}

buf_err_t buf_encode(buf *dst, bool expand, const char *fmt, ...) {
    trace("Starting buf_encode: expand=%d, fmt='%s'", expand, fmt);
    
    if (!dst || !fmt) {
        error("buf_encode: NULL pointer passed");
        return BUF_ERR_NULL_PTR;
    }
    
    if (!dst->data) {
        error("buf_encode: Buffer data is NULL");
        return BUF_ERR_NULL_PTR;
    }
    
    va_list args;
    va_start(args, fmt);
    
    fmt_state_t state = {
        .big_endian = true,  // Default to big endian
        .byte_swap = false,
        .fmt = fmt
    };
    
    buf_err_t result = BUF_OK;
    
    while (*state.fmt && result == BUF_OK) {
        char type;
        int count;
        
        const char* next_pos = parse_next_type(&state, &type, &count);
        if (!next_pos) {
            result = BUF_ERR_BAD_FORMAT;
            break;
        }
        
        if (type == '.') {
            // Write zero bytes
            if (dst->cursor + count > dst->len) {
                if (expand) {
                    size_t new_size = dst->len + count + 1024; // Add some extra space
                    result = buf_resize(dst, new_size);
                    if (result != BUF_OK) {
                        error("Failed to expand buffer for %d skip bytes", count);
                        break;
                    }
                } else {
                    warn("Not enough space to write %d skip bytes", count);
                    result = BUF_ERR_OUT_OF_BOUNDS;
                    break;
                }
            }
            memset(dst->data + dst->cursor, 0, count);
            dst->cursor += count;
            trace("Wrote %d zero bytes", count);
            continue;
        }
        
        if (type == 'B') {
            // Bit array type
            if (count <= 0) {
                warn("Invalid bit count for bit array: %d", count);
                result = BUF_ERR_BAD_FORMAT;
                break;
            }
            
            // Calculate bytes needed for bit count
            size_t bytes_needed = (count + 7) / 8;
            
            // Check/expand buffer space
            if (dst->cursor + bytes_needed > dst->len) {
                if (expand) {
                    size_t new_size = dst->cursor + bytes_needed + 1024;
                    result = buf_resize(dst, new_size);
                    if (result != BUF_OK) {
                        error("Failed to expand buffer for bit array");
                        break;
                    }
                } else {
                    warn("Not enough space for bit array: need %zu bytes, have %zu", bytes_needed, dst->len - dst->cursor);
                    result = BUF_ERR_OUT_OF_BOUNDS;
                    break;
                }
            }
            
            // Get the source bits
            const bool *src_bits = va_arg(args, const bool*);
            if (!src_bits) {
                error("NULL source pointer for bit array");
                result = BUF_ERR_NULL_PTR;
                break;
            }
            
            // Clear the destination bytes first
            memset(dst->data + dst->cursor, 0, bytes_needed);
            
            // Pack bits into bytes (LSB first)
            for (int bit = 0; bit < count; bit++) {
                if (src_bits[bit]) {
                    int byte_idx = bit / 8;
                    int bit_idx = bit % 8;
                    dst->data[dst->cursor + byte_idx] |= (1 << bit_idx);
                }
            }
            
            dst->cursor += bytes_needed;
            trace("Encoded bit array: %d bits (%zu bytes)", count, bytes_needed);
            
        } else if (type == 'u' || type == 'i' || type == 'f') {
            int size;
            bool is_signed, is_float;
            
            result = parse_type_size(&state, type, &size, &is_signed, &is_float);
            if (result != BUF_OK) break;
            
            // Calculate total bytes needed
            size_t total_bytes = size * count;
            
            // Check/expand buffer space
            if (dst->cursor + total_bytes > dst->len) {
                if (expand) {
                    size_t new_size = dst->cursor + total_bytes + 1024; // Add extra space
                    result = buf_resize(dst, new_size);
                    if (result != BUF_OK) {
                        error("Failed to expand buffer for %zu bytes", total_bytes);
                        break;
                    }
                } else {
                    warn("Not enough space: need %zu bytes, have %zu", total_bytes, dst->len - dst->cursor);
                    result = BUF_ERR_OUT_OF_BOUNDS;
                    break;
                }
            }
            
            // Handle arrays by reading each value
            for (int i = 0; i < count; i++) {
                uint64_t value = 0;
                double fvalue = 0.0;
                
                if (is_float) {
                    if (size == 4) {
                        float f = (float)va_arg(args, double);  // float promoted to double in varargs
                        fvalue = f;
                        memcpy(&value, &f, sizeof(float));
                    } else if (size == 8) {
                        fvalue = va_arg(args, double);
                        memcpy(&value, &fvalue, sizeof(double));
                    }
                } else {
                    if (size == 1) {
                        value = va_arg(args, int);  // char promoted to int in varargs
                    } else if (size == 2) {
                        value = va_arg(args, int);  // short promoted to int in varargs
                    } else if (size == 4) {
                        value = va_arg(args, uint32_t);
                    } else if (size == 8) {
                        value = va_arg(args, uint64_t);
                    }
                }
                
                // Copy to buffer with proper byte order
                uint8_t temp[8];
                memcpy(temp, &value, size);
                
                // Apply endianness conversion if needed
                if (size > 1) {
                    convert_endianness(temp, size, state.big_endian, state.byte_swap);
                }
                
                memcpy(dst->data + dst->cursor, temp, size);
                dst->cursor += size;
            }
            
            trace("Encoded %d items of type %c%d (total %zu bytes)", count, type, size * 8, total_bytes);
        }
    }
    
    va_end(args);
    
    if (result == BUF_OK) {
        info("buf_encode completed successfully, cursor at %zu", dst->cursor);
    } else {
        error("buf_encode failed with error code %d", result);
    }
    
    return result;
}

buf_err_t buf_extract_buf(buf *src, size_t start_offset, size_t length, buf **extracted) {
    trace("Extracting buffer: offset=%zu, length=%zu", start_offset, length);
    
    if (!src || !extracted) {
        error("buf_extract_buf: NULL pointer passed");
        return BUF_ERR_NULL_PTR;
    }
    
    if (!src->data) {
        error("buf_extract_buf: Source buffer data is NULL");
        return BUF_ERR_NULL_PTR;
    }
    
    // Calculate absolute start position
    size_t abs_start = src->cursor + start_offset;
    
    // Validate bounds
    if (abs_start > src->len) {
        warn("Start offset beyond buffer end: %zu > %zu", abs_start, src->len);
        return BUF_ERR_OUT_OF_BOUNDS;
    }
    
    if (abs_start + length > src->len) {
        warn("Extraction length exceeds buffer: %zu + %zu > %zu", abs_start, length, src->len);
        return BUF_ERR_OUT_OF_BOUNDS;
    }
    
    // Allocate new buffer
    buf_err_t result = buf_alloc(extracted, length);
    if (result != BUF_OK) {
        error("Failed to allocate extraction buffer");
        return result;
    }
    
    // Copy data
    memcpy((*extracted)->data, src->data + abs_start, length);
    (*extracted)->cursor = length; // Set cursor to end of copied data
    
    trace("Successfully extracted %zu bytes from offset %zu", length, start_offset);
    return BUF_OK;
}