#pragma once

/**
 * @file ptk_defs.h
 * @brief Core definitions, types, and platform-specific macros for Protocol Toolkit
 * 
 * This header provides common definitions used throughout the Protocol Toolkit:
 * - Export/import macros for public APIs
 * - Common type definitions and aliases
 * - Platform-specific macros
 * - Standard constants and handles
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

//=============================================================================
// API EXPORT/IMPORT MACROS
//=============================================================================

/**
 * @def PTK_API
 * @brief Macro for marking functions as part of the public API
 * 
 * This macro ensures proper export/import decoration for shared library builds.
 * All public API functions should be decorated with PTK_API.
 */
#if defined(_WIN32) || defined(__CYGWIN__)
#  define PTK_API __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
#  define PTK_API __attribute__((visibility("default")))
#else
#  define PTK_API
#endif

//=============================================================================
// PLATFORM-SPECIFIC MACROS
//=============================================================================

/**
 * @def ptk_thread_local
 * @brief Platform-specific thread-local storage specifier
 */
#if defined(_WIN32) && defined(_MSC_VER)
#define ptk_thread_local __declspec(thread)
#else
#define ptk_thread_local __thread
#endif

//=============================================================================
// STANDARD TYPE ALIASES
//=============================================================================

/**
 * @brief Standard integer type aliases for convenience
 */
typedef uint8_t ptk_u8_t;      /**< Unsigned 8-bit integer */
typedef uint16_t ptk_u16_t;    /**< Unsigned 16-bit integer */
typedef uint32_t ptk_u32_t;    /**< Unsigned 32-bit integer */
typedef uint64_t ptk_u64_t;    /**< Unsigned 64-bit integer */

typedef int8_t ptk_i8_t;       /**< Signed 8-bit integer */
typedef int16_t ptk_i16_t;     /**< Signed 16-bit integer */
typedef int32_t ptk_i32_t;     /**< Signed 32-bit integer */
typedef int64_t ptk_i64_t;     /**< Signed 64-bit integer */

typedef float ptk_f32_t;       /**< 32-bit floating point */
typedef double ptk_f64_t;      /**< 64-bit floating point */

//=============================================================================
// TIME TYPES AND CONSTANTS
//=============================================================================

/**
 * @typedef ptk_time_ms
 * @brief Represents absolute time in milliseconds since the Unix epoch
 */
typedef int64_t ptk_time_ms;

/**
 * @typedef ptk_duration_ms
 * @brief Represents a time duration in milliseconds
 */
typedef int64_t ptk_duration_ms;

/**
 * @def PTK_TIME_WAIT_FOREVER
 * @brief Indicates an infinite timeout duration
 */
#define PTK_TIME_WAIT_FOREVER (INT64_MAX)

/**
 * @def PTK_TIME_NO_WAIT
 * @brief Indicates non-blocking behavior (no wait)
 */
#define PTK_TIME_NO_WAIT (INT64_MIN)

//=============================================================================
// BUFFER TYPES
//=============================================================================

/**
 * @typedef ptk_buf_size_t
 * @brief Type for buffer sizes and indices
 * 
 * Note: This uses uint16_t to match the existing small buffer design
 * in the Protocol Toolkit. Buffers are intended for protocol messages,
 * not large data transfers.
 */
typedef uint16_t ptk_buf_size_t;

//=============================================================================
// ERROR TYPES
//=============================================================================

/**
 * @typedef ptk_err_t
 * @brief Error code enumeration for Protocol Toolkit operations
 */
typedef enum ptk_err_t{
    PTK_OK,                     // Status/error was OK
    PTK_ERR_ABORT,              // The current operation was aborted.
    PTK_ERR_ADDRESS_IN_USE,     // Address already in use
    PTK_ERR_AUTHENTICATION_FAILED, // Authentication failed
    PTK_ERR_AUTHORIZATION_FAILED,  // Authorization failed
    PTK_ERR_BAD_FORMAT,         // Invalid format in a format string
    PTK_ERR_BAD_INTERNAL_STATE, // Internal state is inconsistent
    PTK_ERR_BUSY,               // Resource is busy
    PTK_ERR_CANCELED,           // Operation was canceled
    PTK_ERR_BUFFER_TOO_SMALL,   // Buffer too small for operation
    PTK_ERR_CHECKSUM_FAILED,    // Checksum/CRC verification failed
    PTK_ERR_CLOSED,             // Socket is closed
    PTK_ERR_CONFIGURATION_ERROR,// Configuration error
    PTK_ERR_CONNECTION_REFUSED, // Connection refused by remote
    PTK_ERR_DEVICE_BUSY,        // Device is busy
    PTK_ERR_DEVICE_FAILURE,     // Device failure
    PTK_ERR_HOST_UNREACHABLE,   // Host unreachable
    PTK_ERR_INTERRUPT,          // The current operation was interrupted.
    PTK_ERR_INVALID_PARAM,      // Invalid parameter passed
    PTK_ERR_INVALID_STATE,      // Invalid state for operation
    PTK_ERR_NETWORK_ERROR,      // Network operation failed
    PTK_ERR_OVERFLOW,           // Resource overflow (e.g., ref count)
    PTK_ERR_NO_RESOURCES,       // No resources available, memory, file descriptors etc.
    PTK_ERR_NULL_PTR,           // Null pointer in params or returns
    PTK_ERR_OUT_OF_BOUNDS,      // Index out of bounds
    PTK_ERR_PARSE_ERROR,        // Failed to parse data
    PTK_ERR_PROTOCOL_ERROR,     // Protocol-specific error
    PTK_ERR_RATE_LIMITED,       // Rate limit exceeded
    PTK_ERR_SEQUENCE_ERROR,     // Sequence/ordering error
    PTK_ERR_SIGNAL,             // Socket operation was signaled/interrupted by external thread
    PTK_ERR_TIMEOUT,            // Operation timed out
    PTK_ERR_UNSUPPORTED,        // Operation not supported
    PTK_ERR_UNSUPPORTED_VERSION, // Unsupported protocol version
    PTK_ERR_VALIDATION,         // Validation error
    PTK_ERR_WOULD_BLOCK,        // Operation would block
} ptk_err_t;

//=============================================================================
// SHARED MEMORY TYPES AND CONSTANTS
//=============================================================================

/**
 * @typedef ptk_shared_handle_t
 * @brief Handle for shared memory objects
 */
typedef struct ptk_shared_handle {
    uintptr_t value;  /**< Opaque handle value */
} ptk_shared_handle_t;

/**
 * @def PTK_SHARED_INVALID_HANDLE
 * @brief Invalid shared memory handle constant
 */
#define PTK_SHARED_INVALID_HANDLE ((ptk_shared_handle_t){UINT64_MAX})

/**
 * @def PTK_THREAD_NO_PARENT
 * @brief Constant indicating a thread has no parent
 */
#define PTK_THREAD_NO_PARENT PTK_SHARED_INVALID_HANDLE

//=============================================================================
// SHARED MEMORY VALIDATION AND COMPARISON
//=============================================================================

/**
 * @def PTK_SHARED_IS_VALID
 * @brief Check if a shared handle is valid
 */
#define PTK_SHARED_IS_VALID(handle) ((handle).value != UINT64_MAX)

/**
 * @def PTK_SHARED_IS_INVALID
 * @brief Check if a shared handle is invalid
 */
#define PTK_SHARED_IS_INVALID(handle) ((handle).value == UINT64_MAX)

static inline bool ptk_shared_is_valid(ptk_shared_handle_t handle) {
    return ((handle).value != UINT64_MAX);
}

static inline bool ptk_shared_handle_equal(ptk_shared_handle_t a, ptk_shared_handle_t b) {
    return ((a).value == (b).value);
}
