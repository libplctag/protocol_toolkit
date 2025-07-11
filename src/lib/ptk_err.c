#include "ptk_err.h"

ptk_thread_local ptk_err ptk_current_error = PTK_OK;

const char *ptk_err_to_string(ptk_err err) {
    switch(err) {
        case PTK_OK: return "Success";
        case PTK_ERR_OUT_OF_BOUNDS: return "Index out of bounds";
        case PTK_ERR_NULL_PTR: return "Null pointer in parameters or returns";
        case PTK_ERR_NO_RESOURCES: return "No resources available";
        case PTK_ERR_BAD_FORMAT: return "Invalid format in format string";
        case PTK_ERR_INVALID_PARAM: return "Invalid parameter passed";
        case PTK_ERR_NETWORK_ERROR: return "Network operation failed";
        case PTK_ERR_CLOSED: return "Socket is closed";
        case PTK_ERR_TIMEOUT: return "Operation timed out";
        case PTK_ERR_WOULD_BLOCK: return "Operation would block";
        case PTK_ERR_ADDRESS_IN_USE: return "Address already in use";
        case PTK_ERR_CONNECTION_REFUSED: return "Connection refused by remote";
        case PTK_ERR_HOST_UNREACHABLE: return "Host unreachable";
        case PTK_ERR_PROTOCOL_ERROR: return "Protocol-specific error";
        case PTK_ERR_CHECKSUM_FAILED: return "Checksum/CRC verification failed";
        case PTK_ERR_BUFFER_TOO_SMALL: return "Buffer too small for operation";
        case PTK_ERR_PARSE_ERROR: return "Failed to parse data";
        case PTK_ERR_UNSUPPORTED_VERSION: return "Unsupported protocol version";
        case PTK_ERR_SEQUENCE_ERROR: return "Sequence/ordering error";
        case PTK_ERR_AUTHENTICATION_FAILED: return "Authentication failed";
        case PTK_ERR_AUTHORIZATION_FAILED: return "Authorization failed";
        case PTK_ERR_RATE_LIMITED: return "Rate limit exceeded";
        case PTK_ERR_DEVICE_BUSY: return "Device is busy";
        case PTK_ERR_DEVICE_FAILURE: return "Device failure";
        case PTK_ERR_CONFIGURATION_ERROR: return "Configuration error";
        case PTK_ERR_INTERRUPT: return "The current operation was interrupted";
        case PTK_ERR_ABORT: return "The current operation was aborted";
        case PTK_ERR_VALIDATION: return "Validation error";
        case PTK_ERR_UNSUPPORTED: return "Operation not supported";
        default: return "Unknown error";
    }
}