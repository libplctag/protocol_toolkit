#pragma once

/**
 * @brief Definitions for error handling.
 *
 */

/**
 * Error codes for event loop operations
 */
typedef enum {
    EV_OK,                     // Status/error was OK
    EV_ERR_OUT_OF_BOUNDS,      // Index out of bounds
    EV_ERR_NULL_PTR,           // Null pointer in params or returns
    EV_ERR_NO_RESOURCES,       // No resources available, memory, file descriptors etc.
    EV_ERR_BAD_FORMAT,         // Invalid format in a format string
    EV_ERR_INVALID_PARAM,      // Invalid parameter passed
    EV_ERR_NETWORK_ERROR,      // Network operation failed
    EV_ERR_CLOSED,             // Socket is closed
    EV_ERR_TIMEOUT,            // Operation timed out
    EV_ERR_WOULD_BLOCK,        // Operation would block
    EV_ERR_ADDRESS_IN_USE,     // Address already in use
    EV_ERR_CONNECTION_REFUSED, // Connection refused by remote
    EV_ERR_HOST_UNREACHABLE,   // Host unreachable
} ev_err;

/**
 * Convert error code to human-readable string
 *
 * @param err The error code
 * @return String description of the error
 */
const char* ev_err_to_string(ev_err err);
