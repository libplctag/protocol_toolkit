/**
 * PTK - POSIX Time Implementation
 * 
 * POSIX-compliant time functions for timers and timeouts.
 */

#include "ptk_event.h"
#include <time.h>
#include <sys/time.h>
#include <stdint.h>
#include <errno.h>

/* Forward declarations for error handling */
extern void ptk_clear_error(void);
extern void ptk_set_error_internal_internal(ptk_status_t error);


/**
 * Get current time in milliseconds (platform abstracted)
 * Used internally by timer system
 */
extern uint64_t ptk_get_time_ms(void) {
    struct timespec ts;
    
    /* Try to use CLOCK_MONOTONIC for better accuracy and resilience to time changes */
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        uint64_t ms = (uint64_t)ts.tv_sec * 1000ULL;
        ms += (uint64_t)ts.tv_nsec / 1000000ULL;
        ptk_clear_error();
        return ms;
    }
    
    /* Fallback to gettimeofday if CLOCK_MONOTONIC is not available */
    struct timeval tv;
    if (gettimeofday(&tv, NULL) == 0) {
        uint64_t ms = (uint64_t)tv.tv_sec * 1000ULL;
        ms += (uint64_t)tv.tv_usec / 1000ULL;
        ptk_clear_error();
        return ms;
    }
    
    /* Both methods failed */
    ptk_set_error_internal(PTK_ERROR_TIMEOUT);
    return 0;
}

/**
 * Convert milliseconds to timespec structure
 */
static void ms_to_timespec(uint32_t ms, struct timespec* ts) {
    ts->tv_sec = ms / 1000;
    ts->tv_nsec = (ms % 1000) * 1000000L;
}

/**
 * Convert milliseconds to timeval structure
 */
static void ms_to_timeval(uint32_t ms, struct timeval* tv) {
    tv->tv_sec = ms / 1000;
    tv->tv_usec = (ms % 1000) * 1000;
}

/**
 * Sleep for specified milliseconds
 * Used internally for timeout implementations
 */
static ptk_status_t ptk_sleep_ms(uint32_t ms) {
    struct timespec ts;
    ms_to_timespec(ms, &ts);
    
    while (nanosleep(&ts, &ts) == -1) {
        if (errno == EINTR) {
            /* Interrupted by signal, continue with remaining time */
            continue;
        } else {
            /* Other error */
            return PTK_ERROR_INTERRUPTED;
        }
    }
    
    return PTK_OK;
}

/**
 * Calculate time difference in milliseconds
 */
static uint64_t time_diff_ms(uint64_t start_time, uint64_t end_time) {
    if (end_time >= start_time) {
        return end_time - start_time;
    } else {
        /* Handle wrap-around (though unlikely with 64-bit values) */
        return (UINT64_MAX - start_time) + end_time + 1;
    }
}

/**
 * Check if a timeout has occurred
 */
static bool is_timeout(uint64_t start_time_ms, uint32_t timeout_ms) {
    if (timeout_ms == 0) {
        return false;  /* No timeout specified */
    }
    
    uint64_t current_time = ptk_get_time_ms();
    uint64_t elapsed = time_diff_ms(start_time_ms, current_time);
    
    return elapsed >= timeout_ms;
}

/**
 * Get remaining timeout in milliseconds
 */
static uint32_t get_remaining_timeout_ms(uint64_t start_time_ms, uint32_t timeout_ms) {
    if (timeout_ms == 0) {
        return 0;  /* No timeout */
    }
    
    uint64_t current_time = ptk_get_time_ms();
    uint64_t elapsed = time_diff_ms(start_time_ms, current_time);
    
    if (elapsed >= timeout_ms) {
        return 0;  /* Timeout expired */
    }
    
    return timeout_ms - (uint32_t)elapsed;
}