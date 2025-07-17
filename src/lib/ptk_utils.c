#include "ptk_utils.h"
#include <time.h>
#include <signal.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#include <unistd.h>
#endif

//=============================================================================
// GLOBAL INTERRUPT HANDLER
//=============================================================================

static void (*g_interrupt_handler)(void) = NULL;

static void internal_signal_handler(int sig) {
    if (g_interrupt_handler) {
        g_interrupt_handler();
    }
}

ptk_err_t ptk_set_interrupt_handler(void (*handler)(void)) {
    g_interrupt_handler = handler;
    
    if (handler) {
        // Set up signal handlers for common interrupt signals
        if (signal(SIGINT, internal_signal_handler) == SIG_ERR) {
            return PTK_ERR_CONFIGURATION_ERROR;
        }
        if (signal(SIGTERM, internal_signal_handler) == SIG_ERR) {
            return PTK_ERR_CONFIGURATION_ERROR;
        }
#ifndef _WIN32
        // On Unix-like systems, also handle SIGHUP
        if (signal(SIGHUP, internal_signal_handler) == SIG_ERR) {
            return PTK_ERR_CONFIGURATION_ERROR;
        }
#endif
    } else {
        // Reset signal handlers to default
        signal(SIGINT, SIG_DFL);
        signal(SIGTERM, SIG_DFL);
#ifndef _WIN32
        signal(SIGHUP, SIG_DFL);
#endif
    }
    
    return PTK_OK;
}

//=============================================================================
// TIME FUNCTIONS
//=============================================================================

ptk_time_ms ptk_now_ms(void) {
#ifdef _WIN32
    // Windows implementation
    FILETIME ft;
    ULARGE_INTEGER uli;
    
    GetSystemTimeAsFileTime(&ft);
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    
    // Convert from 100-nanosecond intervals since Jan 1, 1601
    // to milliseconds since Unix epoch (Jan 1, 1970)
    return (ptk_time_ms)((uli.QuadPart - 116444736000000000ULL) / 10000);
#else
    // Unix/Linux/macOS implementation
    struct timespec ts;
    
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        // Fallback to gettimeofday if clock_gettime fails
        struct timeval tv;
        if (gettimeofday(&tv, NULL) != 0) {
            return 0;  // Error case
        }
        return (ptk_time_ms)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
    }
    
    return (ptk_time_ms)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
#endif
}

ptk_err_t ptk_sleep_ms(ptk_duration_ms duration) {
    if (duration < 0) {
        return PTK_ERR_INVALID_PARAM;
    }
    
#ifdef _WIN32
    Sleep((DWORD)duration);
#else
    struct timespec ts;
    ts.tv_sec = duration / 1000;
    ts.tv_nsec = (duration % 1000) * 1000000;
    
    if (nanosleep(&ts, NULL) != 0) {
        return PTK_ERR_NETWORK_ERROR;
    }
#endif
    
    return PTK_OK;
}