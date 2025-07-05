#pragma once

/**
 * @file ev_time.h
 * @brief Timekeeping utilities and interrupt handler configuration.
 */

#include <stdint.h>
#include "ev_err.h"

/**
 * @def EV_TIME_WAIT_FOREVER
 * @brief Indicates an infinite timeout duration.
 */
#define EV_TIME_WAIT_FOREVER (INT64_MAX)

/**
 * @def EV_TIME_NO_WAIT
 * @brief Indicates non-blocking behavior (no wait).
 */
#define EV_TIME_NO_WAIT (INT64_MIN)

/**
 * @typedef ev_time_ms
 * @brief Represents absolute time in milliseconds since the Unix epoch.
 */
typedef int64_t ev_time_ms;

/**
 * @typedef ev_duration_ms
 * @brief Represents a time duration in milliseconds.
 */
typedef int64_t ev_duration_ms;

/**
 * @brief Registers a custom interrupt handler.
 *
 * On POSIX systems, this sets up handlers for signals like SIGTERM and SIGINT.
 * On Windows, it uses equivalent mechanisms to handle console interrupts.
 *
 * @param handler Function pointer to user-defined interrupt handler.
 * @return `EV_OK` on success or an appropriate `ev_err` code on failure.
 */
extern ev_err ev_set_interrupt_handler(void (*handler)(void));

/**
 * @brief Retrieves the current system time in milliseconds since the Unix epoch.
 *
 * @return Time in milliseconds as an `ev_time_ms` value.
 */
extern ev_time_ms ev_now_ms(void);
