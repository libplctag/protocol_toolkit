#pragma once

#include <ptk_thread.h>

/**
 * @brief Definitions for error handling.
 *
 */

/**
 * Error codes for event loop operations
 */
typedef enum {
    PTK_OK,                     // Status/error was OK
    PTK_ERR_OUT_OF_BOUNDS,      // Index out of bounds
    PTK_ERR_NULL_PTR,           // Null pointer in params or returns
    PTK_ERR_NO_RESOURCES,       // No resources available, memory, file descriptors etc.
    PTK_ERR_BAD_FORMAT,         // Invalid format in a format string
    PTK_ERR_INVALID_PARAM,      // Invalid parameter passed
    PTK_ERR_NETWORK_ERROR,      // Network operation failed
    PTK_ERR_CLOSED,             // Socket is closed
    PTK_ERR_TIMEOUT,            // Operation timed out
    PTK_ERR_WOULD_BLOCK,        // Operation would block
    PTK_ERR_ADDRESS_IN_USE,     // Address already in use
    PTK_ERR_CONNECTION_REFUSED, // Connection refused by remote
    PTK_ERR_HOST_UNREACHABLE,   // Host unreachable
    PTK_ERR_PROTOCOL_ERROR,     // Protocol-specific error
    PTK_ERR_CHECKSUM_FAILED,    // Checksum/CRC verification failed
    PTK_ERR_BUFFER_TOO_SMALL,   // Buffer too small for operation
    PTK_ERR_PARSE_ERROR,        // Failed to parse data
    PTK_ERR_UNSUPPORTED_VERSION, // Unsupported protocol version
    PTK_ERR_SEQUENCE_ERROR,     // Sequence/ordering error
    PTK_ERR_AUTHENTICATION_FAILED, // Authentication failed
    PTK_ERR_AUTHORIZATION_FAILED,  // Authorization failed
    PTK_ERR_RATE_LIMITED,       // Rate limit exceeded
    PTK_ERR_DEVICE_BUSY,        // Device is busy
    PTK_ERR_DEVICE_FAILURE,     // Device failure
    PTK_ERR_CONFIGURATION_ERROR,// Configuration error
    PTK_ERR_INTERRUPT,          // The current operation was interrupted.
    PTK_ERR_ABORT,              // The current operation was aborted.
    PTK_ERR_VALIDATION,         // Validation error
    PTK_ERR_SIGNAL,             // Socket operation was signaled/interrupted by external thread
    PTK_ERR_UNSUPPORTED         // Operation not supported
} ptk_err;

extern ptk_thread_local ptk_err ptk_current_error;

/**
 * @brief Set the current error code
 *
 * This sets the current error code for the thread. It can be retrieved later using `ptk_get_err()`.
 *
 * @param err The error code to set
*/
static inline void ptk_set_err(ptk_err err) {
    ptk_current_error = err;
}

/**
 * @brief Get the current error code
 *
 * This retrieves the current error code for the thread. If no error has occurred, it returns PTK_OK.
 *
 * @return The current error code
 */
static inline ptk_err ptk_get_err(void) {
    return ptk_current_error;
}

/**
 * Convert error code to human-readable string
 *
 * @param err The error code
 * @return String description of the error
 */
const char* ptk_err_to_string(ptk_err err);
