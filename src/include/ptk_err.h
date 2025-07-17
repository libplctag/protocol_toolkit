#pragma once

#include <ptk_defs.h>

/**
 * @brief Definitions for error handling.
 *
 */

extern ptk_thread_local ptk_err_t ptk_current_error;

/**
 * @brief Set the current error code
 *
 * This sets the current error code for the thread. It can be retrieved later using `ptk_get_err()`.
 *
 * @param err The error code to set
*/
static inline void ptk_set_err(ptk_err_t err) {
    ptk_current_error = err;
}

/**
 * @brief Get the current error code
 *
 * This retrieves the current error code for the thread. If no error has occurred, it returns PTK_OK.
 *
 * @return The current error code
 */
static inline ptk_err_t ptk_get_err(void) {
    return ptk_current_error;
}

/**
 * Convert error code to human-readable string
 *
 * @param err The error code
 * @return String description of the error
 */
PTK_API const char* ptk_err_to_string(ptk_err_t err);

