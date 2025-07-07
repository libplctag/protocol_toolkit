#pragma once

/**
 * @file ptk_buf.h
 * @brief Safe buffer API using type-safe arrays
 *
 * Buffer management using safe byte arrays with start/end pointers
 * for stream processing operations.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "ptk_err.h"
#include "ptk_array.h"

//=============================================================================
// BUFFER STRUCTURE
//=============================================================================

/**
 * @brief Buffer structure using safe byte array
 */
typedef struct ptk_buf_t {
    u8_array_t *data;           // Safe byte array (owns capacity info)
    size_t start;               // Start position for reading
    size_t end;                 // End position (exclusive)
} ptk_buf_t;

//=============================================================================
// BUFFER OPERATIONS
//=============================================================================

/**
 * @brief Initialize buffer from existing byte array.
 *
 * Sets start and end to zero. The buffer references the provided array.
 *
 * @param buf Pointer to buffer to initialize
 * @param data Pointer to existing u8_array_t
 * @return PTK_OK on success, error code on failure
 */
ptk_err ptk_buf_make(ptk_buf_t *buf, u8_array_t *data);

/**
 * @brief Create buffer with new byte array of specified size.
 *
 * Allocates a new u8_array_t and initializes buffer to reference it.
 *
 * @param alloc Allocator to use for buffer creation
 * @param buf Pointer to store allocated buffer pointer
 * @param size Initial array size
 * @return PTK_OK on success, error code on failure
 */
ptk_err ptk_buf_create(ptk_allocator_t *alloc, ptk_buf_t **buf, size_t size);

/**
 * @brief Gets the amount of data between start and end.
 *
 * Note that the end index is exclusive.
 *
 * @param len
 * @param buf
 * @return ptk_err
 */
ptk_err ptk_buf_len(size_t *len, ptk_buf_t *buf);

/**
 * @brief Get the capacity of the buffer.
 *
 * Returns the capacity of the underlying byte array.
 *
 * @param cap Pointer to store capacity
 * @param buf Buffer to query
 * @return ptk_err
 */
ptk_err ptk_buf_cap(size_t *cap, ptk_buf_t *buf);

/**
 * @brief Get the current start position
 *
 * @param start
 * @param buf
 * @return ptk_err
 */
ptk_err ptk_buf_get_start(size_t *start, ptk_buf_t *buf);

/**
 * @brief Get a data pointer to the start index
 *
 * @param ptr
 * @param buf
 * @return ptk_err
 */
ptk_err ptk_buf_get_start_ptr(uint8_t **ptr, ptk_buf_t *buf);

/**
 * @brief Set the start positions
 *
 * Sets the start position to anywhere between 0 and end.
 *
 * @param start
 * @param buf
 * @return ptk_err
 */
ptk_err ptk_buf_set_start(ptk_buf_t *buf, size_t start);

/**
 * @brief Get the current end position
 *
 *
 * @param end
 * @param buf
 * @return ptk_err
 */
ptk_err ptk_buf_get_end(size_t *end, ptk_buf_t *buf);

/**
 * @brief Get a data pointer to the end index.
 *
 * @param ptr
 * @param buf
 * @return ptk_err
 */
ptk_err ptk_buf_get_end_ptr(uint8_t **ptr, ptk_buf_t *buf);

/**
 * @brief Set the end position
 *
 * Sets the end position to anywhere between start and capacity.
 *
 * @param buf
 * @param end
 * @return ptk_err
 */
ptk_err ptk_buf_set_end(ptk_buf_t *buf, size_t end);

/**
 * @brief Get the remaining space from the end index to the capacity of the buffer.
 *
 * @param remaining
 * @param buf
 * @return ptk_err
 */
ptk_err ptk_buf_get_remaining(size_t *remaining, ptk_buf_t *buf);

/**
 * @brief Moves the data between start and end to the new start position
 *
 * This moves the data between start and end to the new start position.  It updates
 * start to the new start and updates end so that it is correct with the new position
 * of the data within the buffer.
 *
 * @param buf
 * @param new_start
 * @return ptk_err OK on success, OUT_OF_BOUNDs if the move would truncate data.
 */
ptk_err ptk_buf_move_to(ptk_buf_t *buf, size_t new_start);


//=============================================================================
// BUFFER UTILITIES
//=============================================================================

/**
 * @brief Get pointer to data at current start position
 * @param buf Buffer to query
 * @param ptr Pointer to store data pointer
 * @return PTK_OK on success, error code on failure
 */
ptk_err ptk_buf_get_data_ptr(ptk_buf_t *buf, uint8_t **ptr);

/**
 * @brief Advance start position by specified bytes
 * @param buf Buffer to modify
 * @param bytes Number of bytes to advance
 * @return PTK_OK on success, error code on failure
 */
ptk_err ptk_buf_advance_start(ptk_buf_t *buf, size_t bytes);

/**
 * @brief Advance end position by specified bytes
 * @param buf Buffer to modify  
 * @param bytes Number of bytes to advance
 * @return PTK_OK on success, error code on failure
 */
ptk_err ptk_buf_advance_end(ptk_buf_t *buf, size_t bytes);

/**
 * @brief Reset buffer to empty state (start = end = 0)
 * @param buf Buffer to reset
 * @return PTK_OK on success, error code on failure
 */
ptk_err ptk_buf_reset(ptk_buf_t *buf);
