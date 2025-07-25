#include "ptk_types.h"
#include "ptk_log.h"

const char *ptk_error_string(ptk_error_t error_code) {
    switch(error_code) {
        case PTK_OK: return "OK";
        case PTK_ERROR_INVALID_PARAM: return "Invalid parameter";
        case PTK_ERROR_OUT_OF_MEMORY: return "Out of memory";
        case PTK_ERROR_BUFFER_TOO_SMALL: return "Buffer too small";
        case PTK_ERROR_SOCKET_CREATE: return "Socket creation failed";
        case PTK_ERROR_CONNECT: return "Connection failed";
        case PTK_ERROR_TIMEOUT: return "Operation timed out";
        case PTK_ERROR_THREAD_CREATE: return "Thread creation failed";
        case PTK_ERROR_DNS_RESOLVE: return "DNS resolution failed";
        case PTK_ERROR_PROTOCOL: return "Protocol error";
        case PTK_ERROR_INVALID_DATA: return "Invalid data format";
        case PTK_ERROR_NOT_CONNECTED: return "Not connected";
        case PTK_ERROR_ALREADY_CONNECTED: return "Already connected";
        case PTK_ERROR_INTERRUPTED: return "Operation interrupted";
        default: return "Unknown error";
    }
}
