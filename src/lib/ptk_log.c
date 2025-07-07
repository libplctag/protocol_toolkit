#include "ptk_log.h"
#include "ptk_atomic.h"
#include "ptk_utils.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

//=============================================================================
// GLOBALS
//=============================================================================

static ptk_atomic ptk_log_level global_debug_level = PTK_LOG_LEVEL_INFO;

//=============================================================================
// LOG LEVEL MANAGEMENT
//=============================================================================

ptk_log_level ptk_log_level_set(ptk_log_level level) {
    ptk_log_level old_level;
    ptk_atomic_load_u8((uint8_t *)&old_level, (ptk_atomic uint8_t *)&global_debug_level);
    ptk_atomic_store_u8((ptk_atomic uint8_t *)&global_debug_level, (uint8_t)level);
    return old_level;
}

ptk_log_level ptk_log_level_get(void) {
    ptk_log_level level;
    ptk_atomic_load_u8((uint8_t *)&level, (ptk_atomic uint8_t *)&global_debug_level);
    return level;
}

//=============================================================================
// LOGGING FUNCTIONS
//=============================================================================

static const char *log_level_string(ptk_log_level level) {
    switch(level) {
        case PTK_LOG_LEVEL_NONE: return "NONE";
        case PTK_LOG_LEVEL_ERROR: return "ERROR";
        case PTK_LOG_LEVEL_WARN: return "WARN";
        case PTK_LOG_LEVEL_INFO: return "INFO";
        case PTK_LOG_LEVEL_DEBUG: return "DEBUG";
        case PTK_LOG_LEVEL_TRACE: return "TRACE";
        default: return "UNKNOWN";
    }
}

void ptk_log_impl(const char *func, int line_num, ptk_log_level log_level, const char *tmpl, ...) {
    if(log_level > ptk_log_level_get()) {
        return;  // Log level filtered out
    }

    // Get current time
    ptk_time_ms now = ptk_now_ms();
    time_t time_sec = now / 1000;
    int time_ms = now % 1000;

    struct tm *tm_info = localtime(&time_sec);
    if(!tm_info) { return; }

    // Print timestamp and level
    printf("[%04d-%02d-%02d %02d:%02d:%02d.%03d] [%s] %s:%d: ", tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
           tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec, time_ms, log_level_string(log_level), func, line_num);

    // Print the actual message
    va_list args;
    va_start(args, tmpl);
    vprintf(tmpl, args);
    va_end(args);

    printf("\n");
    fflush(stdout);
}

void ptk_log_buf_impl(const char *func, int line_num, ptk_log_level log_level, ptk_buf_t *data) {
    if(log_level > ptk_log_level_get()) {
        return;  // Log level filtered out
    }

    if(!data) {
        ptk_log_impl(func, line_num, log_level, "Buffer data: (null)");
        return;
    }

    size_t data_len;
    if(ptk_buf_len(&data_len, data) != PTK_OK) {
        ptk_log_impl(func, line_num, log_level, "Buffer data: (error getting length)");
        return;
    }

    ptk_log_impl(func, line_num, log_level, "Buffer data (%zu bytes):", data_len);

    if(data_len == 0) {
        printf("  (no data)\n");
        return;
    }

    uint8_t *ptr;
    if(ptk_buf_get_start_ptr(&ptr, data) != PTK_OK) {
        printf("  (error getting data pointer)\n");
        return;
    }

    for(size_t i = 0; i < data_len; i += 16) {
        printf("  %04zx: ", i);

        // Print hex bytes
        for(size_t j = 0; j < 16; j++) {
            if(i + j < data_len) {
                printf("%02x ", ptr[i + j]);
            } else {
                printf("   ");
            }
            if(j == 7) { printf(" "); }
        }

        printf(" |");

        // Print ASCII representation
        for(size_t j = 0; j < 16 && i + j < data_len; j++) {
            uint8_t c = ptr[i + j];
            printf("%c", (c >= 32 && c <= 126) ? c : '.');
        }

        printf("|\n");
    }

    fflush(stdout);
}