/**
 * @file ptk_timer.h
 * @brief Simple, global timer API for protocol toolkit
 *
 * This provides a minimal timer interface for single-protocol applications.
 * The timer manager is global and internal, integrated with the socket event framework.
 *
 * Copyright (c) 2025 Protocol Toolkit Project
 * Licensed under MPL 2.0
 */

#ifndef PTK_TIMER_H
#define PTK_TIMER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Timer callback function type
 * @param user_data User data passed when timer was created
 * @return true to reschedule timer, false to stop
 */
typedef bool (*ptk_timer_callback_t)(void *user_data);

/**
 * @brief Opaque timer handle
 */
typedef struct ptk_timer *ptk_timer_t;

/**
 * @brief Initialize the global timer system
 *
 * This is called automatically by the socket event framework.
 * Applications should not call this directly.
 *
 * @return 0 on success, negative error code on failure
 */
int ptk_timer_init(void);

/**
 * @brief Cleanup the global timer system
 *
 * This is called automatically by the socket event framework.
 * Applications should not call this directly.
 */
void ptk_timer_cleanup(void);

/**
 * @brief Create a one-shot timer
 *
 * @param timeout_ms Timeout in milliseconds
 * @param callback Function to call when timer expires
 * @param user_data User data to pass to callback
 * @return Timer handle, or NULL on failure
 */
ptk_timer_t ptk_timer_create_oneshot(uint32_t timeout_ms,
                                     ptk_timer_callback_t callback,
                                     void *user_data);

/**
 * @brief Create a repeating timer
 *
 * @param interval_ms Interval in milliseconds
 * @param callback Function to call on each interval
 * @param user_data User data to pass to callback
 * @return Timer handle, or NULL on failure
 */
ptk_timer_t ptk_timer_create_repeating(uint32_t interval_ms,
                                       ptk_timer_callback_t callback,
                                       void *user_data);

/**
 * @brief Cancel a timer
 *
 * @param timer Timer to cancel
 * @return 0 on success, negative error code on failure
 */
int ptk_timer_cancel(ptk_timer_t timer);

/**
 * @brief Check if a timer is active
 *
 * @param timer Timer to check
 * @return true if timer is active, false otherwise
 */
bool ptk_timer_is_active(ptk_timer_t timer);

/**
 * @brief Get time until next timer expiration
 *
 * Used internally by the event framework to determine poll/select timeout.
 * Applications should not call this directly.
 *
 * @return Time in milliseconds until next timer, or -1 if no timers active
 */
int ptk_timer_get_next_timeout(void);

/**
 * @brief Process expired timers
 *
 * Called internally by the event framework when timers expire.
 * Applications should not call this directly.
 *
 * @return Number of timers processed
 */
int ptk_timer_process_expired(void);

#ifdef __cplusplus
}
#endif

#endif /* PTK_TIMER_H */
