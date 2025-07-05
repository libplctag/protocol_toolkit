#include "log.h"
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

static ev_log_level current_log_level = EV_LOG_LEVEL_INFO;

ev_log_level ev_log_level_set(ev_log_level level) {
    ev_log_level old_level = current_log_level;
    current_log_level = level;
    return old_level;
}

ev_log_level ev_log_level_get(void) { return current_log_level; }

void ev_log_impl(const char *func, int line_num, ev_log_level log_level, const char *tmpl, ...) {
    if(log_level > current_log_level) { return; }

    const char *level_str;
    switch(log_level) {
        case EV_LOG_LEVEL_ERROR: level_str = "ERROR"; break;
        case EV_LOG_LEVEL_WARN: level_str = "WARN"; break;
        case EV_LOG_LEVEL_INFO: level_str = "INFO"; break;
        case EV_LOG_LEVEL_DEBUG: level_str = "DEBUG"; break;
        case EV_LOG_LEVEL_TRACE: level_str = "TRACE"; break;
        default: level_str = "UNKNOWN"; break;
    }

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

    printf("[%s] %s %s:%d - ", time_str, level_str, func, line_num);

    va_list args;
    va_start(args, tmpl);
    vprintf(tmpl, args);
    va_end(args);

    printf("\n");
    fflush(stdout);
}

void ev_log_buf_impl(const char *func, int line_num, ev_log_level log_level, void *data, size_t data_len) {
    if(log_level > current_log_level || !data) { return; }

    const char *level_str;
    switch(log_level) {
        case EV_LOG_LEVEL_ERROR: level_str = "ERROR"; break;
        case EV_LOG_LEVEL_WARN: level_str = "WARN"; break;
        case EV_LOG_LEVEL_INFO: level_str = "INFO"; break;
        case EV_LOG_LEVEL_DEBUG: level_str = "DEBUG"; break;
        case EV_LOG_LEVEL_TRACE: level_str = "TRACE"; break;
        default: level_str = "UNKNOWN"; break;
    }

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

    printf("[%s] %s %s:%d - Buffer (%zu bytes): ", time_str, level_str, func, line_num, data_len);

    uint8_t *bytes = (uint8_t *)data;
    for(size_t i = 0; i < data_len; i++) {
        printf("%02X ", bytes[i]);
        if((i + 1) % 16 == 0 && i < data_len - 1) { printf("\n                                                     "); }
    }

    printf("\n");
    fflush(stdout);
}