#include "ptk_buf.h"
#include "ptk_log.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>

//=============================================================================
// NON-INLINE BUFFER FUNCTIONS
//=============================================================================

u8 *ptk_buf_get_start_ptr(ptk_buf_t *buf) {
    if(!buf) { 
        error("ptk_buf_get_start_ptr called with NULL buffer");
        return NULL; 
    }

    return buf->data + buf->start;
}

ptk_err ptk_buf_set_start(ptk_buf_t *buf, size_t start) {
    if(!buf) { 
        error("ptk_buf_set_start called with NULL buffer");
        return PTK_ERR_NULL_PTR; 
    }

    if(start > buf->end) { 
        error("ptk_buf_set_start: start %zu > end %zu", start, buf->end);
        return PTK_ERR_OUT_OF_BOUNDS; 
    }

    buf->start = start;
    trace("Buffer start set to %zu", start);
    return PTK_OK;
}

//=============================================================================
// BUFFER PRODUCE/CONSUME FUNCTIONS
//=============================================================================

/**
 * @brief Parse format string and determine required size
 */
static size_t calculate_format_size(const char *fmt, va_list args) {
    size_t total_size = 0;
    const char *p = fmt;
    bool little_endian = true;  // Default endianness
    
    while (*p) {
        if (*p == ' ') {
            p++;
            continue;
        }
        
        if (*p == '<') {
            little_endian = true;
            p++;
            continue;
        }
        
        if (*p == '>') {
            little_endian = false;
            p++;
            continue;
        }
        
        bool is_array = false;
        if (*p == '*') {
            is_array = true;
            p++;
        }
        
        size_t element_size = 0;
        switch (*p) {
            case 'b': element_size = 1; break;
            case 'w': element_size = 2; break;
            case 'd': element_size = 4; break;
            case 'q': element_size = 8; break;
            default:
                error("Unknown format character '%c'", *p);
                return 0;
        }
        
        if (is_array) {
            size_t count = va_arg(args, size_t);
            total_size += element_size * count;
            va_arg(args, void*);  // Skip the data pointer
        } else {
            total_size += element_size;
            // Skip the value argument
            if (element_size == 8) {
                va_arg(args, uint64_t);
            } else {
                va_arg(args, uint32_t);
            }
        }
        
        p++;
    }
    
    return total_size;
}

/**
 * @brief Write value to buffer with endianness handling
 */
static ptk_err write_value_to_buffer(u8 *dest, uint64_t value, size_t size, bool little_endian) {
    if (little_endian) {
        for (size_t i = 0; i < size; i++) {
            dest[i] = (value >> (i * 8)) & 0xFF;
        }
    } else {
        for (size_t i = 0; i < size; i++) {
            dest[size - 1 - i] = (value >> (i * 8)) & 0xFF;
        }
    }
    return PTK_OK;
}

/**
 * @brief Read value from buffer with endianness handling
 */
static uint64_t read_value_from_buffer(const u8 *src, size_t size, bool little_endian) {
    uint64_t value = 0;
    
    if (little_endian) {
        for (size_t i = 0; i < size; i++) {
            value |= ((uint64_t)src[i]) << (i * 8);
        }
    } else {
        for (size_t i = 0; i < size; i++) {
            value |= ((uint64_t)src[size - 1 - i]) << (i * 8);
        }
    }
    
    return value;
}

ptk_err ptk_buf_produce(ptk_buf_t *dest, const char *fmt, ...) {
    if (!dest || !fmt) {
        error("ptk_buf_produce called with NULL parameters");
        return PTK_ERR_NULL_PTR;
    }
    
    // Save original buffer state for rollback
    size_t original_end = dest->end;
    ptk_err original_last_err = dest->last_err;
    
    va_list args;
    va_start(args, fmt);
    
    // First pass: calculate required size and validate format
    va_list args_copy;
    va_copy(args_copy, args);
    size_t required_size = calculate_format_size(fmt, args_copy);
    va_end(args_copy);
    
    if (required_size == 0) {
        error("ptk_buf_produce: Failed to calculate format size");
        va_end(args);
        dest->last_err = PTK_ERR_BAD_FORMAT;
        return PTK_ERR_BAD_FORMAT;
    }
    
    // Check if we have enough space
    size_t available_space = dest->capacity - dest->end;
    if (required_size > available_space) {
        error("ptk_buf_produce: Not enough space. Need %zu, have %zu", required_size, available_space);
        va_end(args);
        dest->last_err = PTK_ERR_BUFFER_TOO_SMALL;
        return PTK_ERR_BUFFER_TOO_SMALL;
    }
    
    // Second pass: actually write the data
    const char *p = fmt;
    bool little_endian = true;  // Default endianness
    u8 *write_ptr = dest->data + dest->end;
    ptk_err result = PTK_OK;
    
    while (*p && result == PTK_OK) {
        if (*p == ' ') {
            p++;
            continue;
        }
        
        if (*p == '<') {
            little_endian = true;
            p++;
            continue;
        }
        
        if (*p == '>') {
            little_endian = false;
            p++;
            continue;
        }
        
        bool is_array = false;
        if (*p == '*') {
            is_array = true;
            p++;
        }
        
        size_t element_size = 0;
        switch (*p) {
            case 'b': element_size = 1; break;
            case 'w': element_size = 2; break;
            case 'd': element_size = 4; break;
            case 'q': element_size = 8; break;
            default:
                error("Unknown format character '%c'", *p);
                result = PTK_ERR_BAD_FORMAT;
                break;
        }
        
        if (result != PTK_OK) break;
        
        if (is_array) {
            size_t count = va_arg(args, size_t);
            const void *data = va_arg(args, const void*);
            
            if (!data && count > 0) {
                error("ptk_buf_produce: NULL data pointer for array");
                result = PTK_ERR_NULL_PTR;
                break;
            }
            
            const u8 *src = (const u8*)data;
            for (size_t i = 0; i < count; i++) {
                uint64_t value = read_value_from_buffer(src + i * element_size, element_size, true);
                write_value_to_buffer(write_ptr, value, element_size, little_endian);
                write_ptr += element_size;
            }
        } else {
            uint64_t value;
            if (element_size == 8) {
                value = va_arg(args, uint64_t);
            } else {
                value = va_arg(args, uint32_t);
            }
            
            write_value_to_buffer(write_ptr, value, element_size, little_endian);
            write_ptr += element_size;
        }
        
        p++;
    }
    
    va_end(args);
    
    // Check if operation completed successfully
    if (result == PTK_OK) {
        // Commit the changes
        dest->end += required_size;
        dest->last_err = PTK_OK;
        debug("ptk_buf_produce: Successfully wrote %zu bytes to buffer", required_size);
    } else {
        // Rollback: restore original buffer state
        dest->end = original_end;
        dest->last_err = result;
        warn("ptk_buf_produce: Failed, buffer state restored");
    }
    
    return result;
}

ptk_err ptk_buf_consume(ptk_buf_t *src, bool peek, const char *fmt, ...) {
    if (!src || !fmt) {
        error("ptk_buf_consume called with NULL parameters");
        return PTK_ERR_NULL_PTR;
    }
    
    // Save original buffer state for rollback (even for peek mode, in case of errors)
    size_t original_start = src->start;
    ptk_err original_last_err = src->last_err;
    
    va_list args;
    va_start(args, fmt);
    
    // First pass: calculate required size and validate format
    va_list args_copy;
    va_copy(args_copy, args);
    size_t required_size = calculate_format_size(fmt, args_copy);
    va_end(args_copy);
    
    if (required_size == 0) {
        error("ptk_buf_consume: Failed to calculate format size");
        va_end(args);
        src->last_err = PTK_ERR_BAD_FORMAT;
        return PTK_ERR_BAD_FORMAT;
    }
    
    // Check if we have enough data
    size_t available_data = src->end - src->start;
    if (required_size > available_data) {
        error("ptk_buf_consume: Not enough data. Need %zu, have %zu", required_size, available_data);
        va_end(args);
        src->last_err = PTK_ERR_BUFFER_TOO_SMALL;
        return PTK_ERR_BUFFER_TOO_SMALL;
    }
    
    // Second pass: actually read the data
    const char *p = fmt;
    bool little_endian = true;  // Default endianness
    const u8 *read_ptr = src->data + src->start;
    ptk_err result = PTK_OK;
    
    // Track successful argument writes for potential rollback
    typedef struct {
        void *ptr;
        uint64_t original_value;
        size_t size;
        bool is_pointer;
    } arg_backup_t;
    
    arg_backup_t arg_backups[64];  // Reasonable limit for number of arguments
    size_t num_backups = 0;
    
    while (*p && result == PTK_OK && num_backups < 64) {
        if (*p == ' ') {
            p++;
            continue;
        }
        
        if (*p == '<') {
            little_endian = true;
            p++;
            continue;
        }
        
        if (*p == '>') {
            little_endian = false;
            p++;
            continue;
        }
        
        bool is_array = false;
        if (*p == '*') {
            is_array = true;
            p++;
        }
        
        size_t element_size = 0;
        switch (*p) {
            case 'b': element_size = 1; break;
            case 'w': element_size = 2; break;
            case 'd': element_size = 4; break;
            case 'q': element_size = 8; break;
            default:
                error("Unknown format character '%c'", *p);
                result = PTK_ERR_BAD_FORMAT;
                break;
        }
        
        if (result != PTK_OK) break;
        
        if (is_array) {
            size_t count = va_arg(args, size_t);
            void *dest = va_arg(args, void*);
            
            if (!dest && count > 0) {
                error("ptk_buf_consume: NULL destination pointer for array");
                result = PTK_ERR_NULL_PTR;
                break;
            }
            
            // For arrays, we backup the entire destination memory
            if (dest && count > 0 && num_backups < 63) {
                size_t backup_size = count * element_size;
                u8 *backup_data = malloc(backup_size);
                if (backup_data) {
                    memcpy(backup_data, dest, backup_size);
                    arg_backups[num_backups].ptr = dest;
                    arg_backups[num_backups].original_value = (uintptr_t)backup_data;
                    arg_backups[num_backups].size = backup_size;
                    arg_backups[num_backups].is_pointer = true;
                    num_backups++;
                }
            }
            
            u8 *dest_ptr = (u8*)dest;
            for (size_t i = 0; i < count; i++) {
                uint64_t value = read_value_from_buffer(read_ptr, element_size, little_endian);
                write_value_to_buffer(dest_ptr + i * element_size, value, element_size, true);
                read_ptr += element_size;
            }
        } else {
            uint64_t value = read_value_from_buffer(read_ptr, element_size, little_endian);
            
            if (element_size == 8) {
                uint64_t *dest = va_arg(args, uint64_t*);
                if (dest && num_backups < 63) {
                    // Backup original value
                    arg_backups[num_backups].ptr = dest;
                    arg_backups[num_backups].original_value = *dest;
                    arg_backups[num_backups].size = 8;
                    arg_backups[num_backups].is_pointer = false;
                    num_backups++;
                    *dest = value;
                }
            } else {
                uint32_t *dest = va_arg(args, uint32_t*);
                if (dest && num_backups < 63) {
                    // Backup original value  
                    arg_backups[num_backups].ptr = dest;
                    arg_backups[num_backups].original_value = *dest;
                    arg_backups[num_backups].size = 4;
                    arg_backups[num_backups].is_pointer = false;
                    num_backups++;
                    *dest = (uint32_t)value;
                }
            }
            
            read_ptr += element_size;
        }
        
        p++;
    }
    
    va_end(args);
    
    // Check if operation completed successfully
    if (result == PTK_OK && num_backups < 64) {
        // Success: commit changes to buffer position (if not peeking)
        if (!peek) {
            src->start += required_size;
            debug("ptk_buf_consume: Successfully consumed %zu bytes from buffer", required_size);
        } else {
            trace("ptk_buf_consume: Successfully peeked %zu bytes from buffer", required_size);
        }
        src->last_err = PTK_OK;
        
        // Free any backup data for arrays
        for (size_t i = 0; i < num_backups; i++) {
            if (arg_backups[i].is_pointer) {
                free((void*)arg_backups[i].original_value);
            }
        }
    } else {
        // Failure: rollback all changes
        warn("ptk_buf_consume: Failed, restoring buffer and argument state");
        
        // Restore buffer state
        src->start = original_start;
        src->last_err = result;
        
        // Restore argument values
        for (size_t i = 0; i < num_backups; i++) {
            if (arg_backups[i].is_pointer) {
                // Restore array data
                memcpy(arg_backups[i].ptr, (void*)arg_backups[i].original_value, arg_backups[i].size);
                free((void*)arg_backups[i].original_value);
            } else {
                // Restore scalar value
                if (arg_backups[i].size == 8) {
                    *(uint64_t*)arg_backups[i].ptr = arg_backups[i].original_value;
                } else {
                    *(uint32_t*)arg_backups[i].ptr = (uint32_t)arg_backups[i].original_value;
                }
            }
        }
        
        if (num_backups >= 64) {
            error("ptk_buf_consume: Too many arguments for transaction safety");
            result = PTK_ERR_INVALID_PARAM;
        }
    }
    
    return result;
}