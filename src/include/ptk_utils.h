#pragma once

/**
 * @file ptk_time.h
 * @brief Timekeeping utilities and interrupt handler configuration.
 */

#include <stdint.h>
#include <ptk_err.h>

/**
 * @def PTK_TIME_WAIT_FOREVER
 * @brief Indicates an infinite timeout duration.
 */
#define PTK_TIME_WAIT_FOREVER (INT64_MAX)

/**
 * @def PTK_TIME_NO_WAIT
 * @brief Indicates non-blocking behavior (no wait).
 */
#define PTK_TIME_NO_WAIT (INT64_MIN)

/**
 * @typedef ptk_time_ms
 * @brief Represents absolute time in milliseconds since the Unix epoch.
 */
typedef int64_t ptk_time_ms;

/**
 * @typedef ptk_duration_ms
 * @brief Represents a time duration in milliseconds.
 */
typedef int64_t ptk_duration_ms;

/**
 * @brief Registers a custom interrupt handler.
 *
 * On POSIX systems, this sets up handlers for signals like SIGTERM and SIGINT.
 * On Windows, it uses equivalent mechanisms to handle console interrupts.
 *
 * @param handler Function pointer to user-defined interrupt handler.
 * @return `PTK_OK` on success or an appropriate `ptk_err` code on failure.
 */
extern ptk_err ptk_set_interrupt_handler(void (*handler)(void));

/**
 * @brief Retrieves the current system time in milliseconds since the Unix epoch.
 *
 * @return Time in milliseconds as an `ptk_time_ms` value.
 */
extern ptk_time_ms ptk_now_ms(void);
