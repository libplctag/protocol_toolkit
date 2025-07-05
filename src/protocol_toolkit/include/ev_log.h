#pragma once

/***************************************************************************
 *   @file
 *   @brief Logging interface with configurable log levels, variadic formatting,
 *          and binary buffer diagnostics.
 *
 *   @author Kyle Hayes
 *   @copyright
 *   Available under either the Mozilla Public License version 2.0 or the
 *   GNU LGPL version 2 (or later).
 ***************************************************************************/

#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>

#include "ev_buf.h"
#include "ev_err.h"

#if defined(_WIN32) && defined(_MSC_VER)
/**
 * @brief Define __func__ as __FUNCTION__ for MSVC compatibility.
 */
#    define __func__ __FUNCTION__
#endif

/**
 * @enum ev_log_level
 * @brief Represents the severity level of a log message.
 */
typedef enum {
    EV_LOG_LEVEL_NONE = 0,  /**< Logging disabled. */
    EV_LOG_LEVEL_ERROR,     /**< Error conditions. */
    EV_LOG_LEVEL_WARN,      /**< Warning conditions. */
    EV_LOG_LEVEL_INFO,      /**< Informational messages. */
    EV_LOG_LEVEL_DEBUG,     /**< Debugging messages. */
    EV_LOG_LEVEL_TRACE,     /**< Fine-grained tracing messages. */
    EV_LOG_LEVEL_END,       /**< Sentinel value (not a valid level). */
} ev_log_level;

/**
 * @brief Sets the current global log level.
 *
 * @param level New log level to set.
 * @return The previous log level.
 */
extern ev_log_level ev_log_level_set(ev_log_level level);

/**
 * @brief Gets the current global log level.
 *
 * @return Current log level.
 */
extern ev_log_level ev_log_level_get(void);

/**
 * @brief Internal log function used by log macros.
 *
 * @param func     Name of the calling function (usually `__func__`).
 * @param line_num Line number in the calling source file.
 * @param log_level Severity of the log message.
 * @param tmpl     printf-style format string.
 * @param ...      Additional arguments for the format string.
 */
extern void ev_log_impl(const char *func, int line_num, ev_log_level log_level, const char *tmpl, ...);

/** @def error
 *  @brief Logs a message at ERROR level using variadic arguments.
 */
#define error(...) \
    do { if (ev_log_level_get() <= EV_LOG_LEVEL_ERROR) \
        ev_log_impl(__func__, __LINE__, EV_LOG_LEVEL_ERROR, __VA_ARGS__); } while(0)

/** @def warn
 *  @brief Logs a message at WARN level using variadic arguments.
 */
#define warn(...) \
    do { if (ev_log_level_get() <= EV_LOG_LEVEL_WARN) \
        ev_log_impl(__func__, __LINE__, EV_LOG_LEVEL_WARN, __VA_ARGS__); } while(0)

/** @def info
 *  @brief Logs a message at INFO level using variadic arguments.
 */
#define info(...) \
    do { if (ev_log_level_get() <= EV_LOG_LEVEL_INFO) \
        ev_log_impl(__func__, __LINE__, EV_LOG_LEVEL_INFO, __VA_ARGS__); } while(0)

/** @def debug
 *  @brief Logs a message at DEBUG level using variadic arguments.
 */
#define debug(...) \
    do { if (ev_log_level_get() <= EV_LOG_LEVEL_DEBUG) \
        ev_log_impl(__func__, __LINE__, EV_LOG_LEVEL_DEBUG, __VA_ARGS__); } while(0)

/** @def trace
 *  @brief Logs a message at TRACE level using variadic arguments.
 */
#define trace(...) \
    do { if (ev_log_level_get() <= EV_LOG_LEVEL_TRACE) \
        ev_log_impl(__func__, __LINE__, EV_LOG_LEVEL_TRACE, __VA_ARGS__); } while(0)

/**
 * @brief Internal function to log raw binary buffer data.
 *
 * @param func      Name of the calling function.
 * @param line_num  Line number in the calling source file.
 * @param log_level Log severity.
 * @param data      Buffer containing the data to log.
 */
extern void ev_log_buf_impl(const char *func, int line_num, ev_log_level log_level, ev_buf *data);

/** @def error_buf
 *  @brief Logs a binary buffer at ERROR level.
 */
#define error_buf(data) \
    do { if (ev_log_level_get() <= EV_LOG_LEVEL_ERROR) \
        ev_log_buf_impl(__func__, __LINE__, EV_LOG_LEVEL_ERROR, data); } while(0)

/** @def warn_buf
 *  @brief Logs a binary buffer at WARN level.
 */
#define warn_buf(data) \
    do { if (ev_log_level_get() <= EV_LOG_LEVEL_WARN) \
        ev_log_buf_impl(__func__, __LINE__, EV_LOG_LEVEL_WARN, data); } while(0)

/** @def info_buf
 *  @brief Logs a binary buffer at INFO level.
 */
#define info_buf(data) \
    do { if (ev_log_level_get() <= EV_LOG_LEVEL_INFO) \
        ev_log_buf_impl(__func__, __LINE__, EV_LOG_LEVEL_INFO, data); } while(0)

/** @def debug_buf
 *  @brief Logs a binary buffer at DEBUG level.
 */
#define debug_buf(data) \
    do { if (ev_log_level_get() <= EV_LOG_LEVEL_DEBUG) \
        ev_log_buf_impl(__func__, __LINE__, EV_LOG_LEVEL_DEBUG, data); } while(0)

/** @def trace_buf
 *  @brief Logs a binary buffer at TRACE level.
 */
#define trace_buf(data) \
    do { if (ev_log_level_get() <= EV_LOG_LEVEL_TRACE) \
        ev_log_buf_impl(__func__, __LINE__, EV_LOG_LEVEL_TRACE, data); } while(0)
