#pragma once

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
} ptk_err;

/**
 * Convert error code to human-readable string
 *
 * @param err The error code
 * @return String description of the error
 */
const char* ptk_err_to_string(ptk_err err);
