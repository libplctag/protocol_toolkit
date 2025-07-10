/**
 * @file ptk_timer.c
 * @brief Simple, global timer implementation for protocol toolkit
 *
 * This provides a minimal timer implementation for single-protocol applications.
 * The timer manager is global and internal, integrated with the socket event framework.
 *
 * Copyright (c) 2025 Protocol Toolkit Project
 * Licensed under MPL 2.0
 */

#include "ptk_timer.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#ifdef DEBUG
    #include <stdio.h>
    #define DEBUG_PRINT(fmt, ...) fprintf(stderr, "[PTK_TIMER] " fmt "\n", ##__VA_ARGS__)
#else
    #define DEBUG_PRINT(fmt, ...) \
        do {                      \
        } while(0)
#endif

/** Maximum number of timers (keep it simple) */
#define MAX_TIMERS 64

/** Timer structure */
struct ptk_timer {
    uint32_t timeout_ms;           /**< Timeout in milliseconds */
    uint64_t expire_time;          /**< Absolute expiration time */
    ptk_timer_callback_t callback; /**< Callback function */
    void *user_data;               /**< User data */
    bool repeating;                /**< True if repeating timer */
    bool active;                   /**< True if timer is active */
    int id;                        /**< Timer ID for debugging */
};

/** Global timer manager */
static struct {
    struct ptk_timer timers[MAX_TIMERS];
    bool initialized;
    int next_id;
} g_timer_mgr;

/**
 * @brief Get current time in milliseconds
 * @return Current time in milliseconds
 */
static uint64_t get_current_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec / 1000;
}

/**
 * @brief Find a free timer slot
 * @return Pointer to free timer, or NULL if none available
 */
static struct ptk_timer *find_free_timer(void) {
    for(int i = 0; i < MAX_TIMERS; i++) {
        if(!g_timer_mgr.timers[i].active) { return &g_timer_mgr.timers[i]; }
    }
    return NULL;
}

int ptk_timer_init(void) {
    if(g_timer_mgr.initialized) {
        DEBUG_PRINT("Timer system already initialized");
        return 0;
    }

    memset(&g_timer_mgr, 0, sizeof(g_timer_mgr));
    g_timer_mgr.initialized = true;
    g_timer_mgr.next_id = 1;

    DEBUG_PRINT("Timer system initialized");
    return 0;
}

void ptk_timer_cleanup(void) {
    if(!g_timer_mgr.initialized) { return; }

    // Cancel all active timers
    for(int i = 0; i < MAX_TIMERS; i++) {
        if(g_timer_mgr.timers[i].active) {
            g_timer_mgr.timers[i].active = false;
            DEBUG_PRINT("Canceled timer %d during cleanup", g_timer_mgr.timers[i].id);
        }
    }

    memset(&g_timer_mgr, 0, sizeof(g_timer_mgr));
    DEBUG_PRINT("Timer system cleaned up");
}

ptk_timer_t ptk_timer_create_oneshot(uint32_t timeout_ms, ptk_timer_callback_t callback, void *user_data) {
    if(!g_timer_mgr.initialized) {
        DEBUG_PRINT("Timer system not initialized");
        return NULL;
    }

    if(!callback || timeout_ms == 0) {
        DEBUG_PRINT("Invalid parameters: callback=%p, timeout_ms=%u", callback, timeout_ms);
        return NULL;
    }

    struct ptk_timer *timer = find_free_timer();
    if(!timer) {
        DEBUG_PRINT("No free timer slots available");
        return NULL;
    }

    uint64_t current_time = get_current_time_ms();

    timer->timeout_ms = timeout_ms;
    timer->expire_time = current_time + timeout_ms;
    timer->callback = callback;
    timer->user_data = user_data;
    timer->repeating = false;
    timer->active = true;
    timer->id = g_timer_mgr.next_id++;

    DEBUG_PRINT("Created one-shot timer %d: timeout=%ums, expires=%" PRIu64, timer->id, timeout_ms, timer->expire_time);

    return timer;
}

ptk_timer_t ptk_timer_create_repeating(uint32_t interval_ms, ptk_timer_callback_t callback, void *user_data) {
    if(!g_timer_mgr.initialized) {
        DEBUG_PRINT("Timer system not initialized");
        return NULL;
    }

    if(!callback || interval_ms == 0) {
        DEBUG_PRINT("Invalid parameters: callback=%p, interval_ms=%u", callback, interval_ms);
        return NULL;
    }

    struct ptk_timer *timer = find_free_timer();
    if(!timer) {
        DEBUG_PRINT("No free timer slots available");
        return NULL;
    }

    uint64_t current_time = get_current_time_ms();

    timer->timeout_ms = interval_ms;
    timer->expire_time = current_time + interval_ms;
    timer->callback = callback;
    timer->user_data = user_data;
    timer->repeating = true;
    timer->active = true;
    timer->id = g_timer_mgr.next_id++;

    DEBUG_PRINT("Created repeating timer %d: interval=%ums, expires=%" PRIu64, timer->id, interval_ms, timer->expire_time);

    return timer;
}

int ptk_timer_cancel(ptk_timer_t timer) {
    if(!g_timer_mgr.initialized) {
        DEBUG_PRINT("Timer system not initialized");
        return -EINVAL;
    }

    if(!timer || !timer->active) {
        DEBUG_PRINT("Invalid or inactive timer");
        return -EINVAL;
    }

    DEBUG_PRINT("Canceling timer %d", timer->id);
    timer->active = false;
    return 0;
}

bool ptk_timer_is_active(ptk_timer_t timer) {
    if(!g_timer_mgr.initialized || !timer) { return false; }
    return timer->active;
}

int ptk_timer_get_next_timeout(void) {
    if(!g_timer_mgr.initialized) { return -1; }

    uint64_t current_time = get_current_time_ms();
    uint64_t next_expire = UINT64_MAX;
    bool found_active = false;

    // Find the earliest expiration time
    for(int i = 0; i < MAX_TIMERS; i++) {
        if(g_timer_mgr.timers[i].active) {
            found_active = true;
            if(g_timer_mgr.timers[i].expire_time < next_expire) { next_expire = g_timer_mgr.timers[i].expire_time; }
        }
    }

    if(!found_active) {
        return -1;  // No active timers
    }

    // Calculate timeout
    if(next_expire <= current_time) {
        return 0;  // Already expired
    }

    uint64_t timeout = next_expire - current_time;
    if(timeout > INT32_MAX) {
        return INT32_MAX;  // Cap at maximum int value
    }

    return (int)timeout;
}

int ptk_timer_process_expired(void) {
    if(!g_timer_mgr.initialized) { return 0; }

    uint64_t current_time = get_current_time_ms();
    int processed_count = 0;

    for(int i = 0; i < MAX_TIMERS; i++) {
        struct ptk_timer *timer = &g_timer_mgr.timers[i];

        if(!timer->active || timer->expire_time > current_time) { continue; }

        DEBUG_PRINT("Processing expired timer %d", timer->id);
        processed_count++;

        // Call the callback
        bool reschedule = timer->callback(timer->user_data);

        if(timer->repeating && reschedule) {
            // Reschedule repeating timer
            timer->expire_time = current_time + timer->timeout_ms;
            DEBUG_PRINT("Rescheduled repeating timer %d for %" PRIu64, timer->id, timer->expire_time);
        } else {
            // One-shot timer or callback returned false
            timer->active = false;
            DEBUG_PRINT("Timer %d completed", timer->id);
        }
    }

    return processed_count;
}
