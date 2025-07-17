#pragma once

/**
 * @file ptk_time.h
 * @brief Timekeeping utilities and interrupt handler configuration.
 */

#include <ptk_defs.h>

/**
 * @brief Registers a custom interrupt handler.
 *
 * On POSIX systems, this sets up handlers for signals like SIGTERM and SIGINT.
 * On Windows, it uses equivalent mechanisms to handle console interrupts.
 *
 * @param handler Function pointer to user-defined interrupt handler.
 * @return `PTK_OK` on success or an appropriate `ptk_err_t` code on failure.
 */
PTK_API ptk_err_t ptk_set_interrupt_handler(void (*handler)(void));

/**
 * @brief Retrieves the current system time in milliseconds since the Unix epoch.
 *
 * @return Time in milliseconds as an `ptk_time_ms` value.
 */
PTK_API ptk_time_ms ptk_now_ms(void);


/**
 * @brief Sleeps for the specified duration in milliseconds.
 *
 * This function blocks the current thread for the given duration.
 *
 * @param duration Duration to sleep in milliseconds.
 * @return `PTK_OK` on success or an appropriate `ptk_err_t` code on failure.
 */
PTK_API ptk_err_t ptk_sleep_ms(ptk_duration_ms duration);
