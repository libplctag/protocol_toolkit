/***************************************************************************
 *   Copyright (C) 2025 by Kyle Hayes                                      *
 *   Author Kyle Hayes  kyle.hayes@gmail.com                               *
 *                                                                         *
 * This software is available under either the Mozilla Public License      *
 * version 2.0 or the GNU LGPL version 2 (or later) license, whichever     *
 * you choose.                                                             *
 *                                                                         *
 * MPL 2.0:                                                                *
 *                                                                         *
 *   This Source Code Form is subject to the terms of the Mozilla Public   *
 *   License, v. 2.0. If a copy of the MPL was not distributed with this   *
 *   file, You can obtain one at http://mozilla.org/MPL/2.0/.              *
 *                                                                         *
 *                                                                         *
 * LGPL 2:                                                                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Library General Public License as       *
 *   published by the Free Software Foundation; either version 2 of the    *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this program; if not, write to the                 *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ptk_atomic.h"
#include "ptk_buf.h"
#include "ptk_err.h"
#include "ptk_log.h"
#include "ptk_thread.h"
#include "ptk_utils.h"


/*
 * Debugging support.
 */


static ptk_atomic uint8_t global_debug_level = PTK_LOG_LEVEL_NONE;
static ptk_atomic uint32_t thread_num = 1;

static ptk_thread_local uint32_t this_thread_num = 0;

static const char *log_level_name[] = {
    [PTK_LOG_LEVEL_NONE] = "NONE", [PTK_LOG_LEVEL_ERROR] = "ERROR", [PTK_LOG_LEVEL_WARN] = "WARN",
    [PTK_LOG_LEVEL_INFO] = "INFO", [PTK_LOG_LEVEL_DEBUG] = "DEBUG", [PTK_LOG_LEVEL_TRACE] = "TRACE",
    [PTK_LOG_LEVEL_END] = "END"};


static uint32_t get_thread_id(void) {
    if(!this_thread_num) { this_thread_num = ptk_atomic_add_fetch_u32(&thread_num, 1); }

    return this_thread_num;
}


/**
 * @brief Sets the current global log level.
 *
 * @param level New log level to set.
 * @return The previous log level.
 */
extern ptk_log_level ptk_log_level_set(ptk_log_level level) {
    ptk_log_level old_level = ptk_atomic_load_u8(&global_debug_level);

    ptk_atomic_store_u8(&global_debug_level, level);

    return old_level;
}

/**
 * @brief Gets the current global log level.
 *
 * @return Current log level.
 */
extern ptk_log_level ptk_log_level_get(void) { return ptk_atomic_load_u8(&global_debug_level); }

/**
 * @brief Internal log function used by log macros.
 *
 * @param func     Name of the calling function (usually `__func__`).
 * @param line_num Line number in the calling source file.
 * @param log_level Severity of the log message.
 * @param tmpl     printf-style format string.
 * @param ...      Additional arguments for the format string.
 */
extern void ptk_log_impl(const char *func, int line_num, ptk_log_level log_level, const char *tmpl, ...) {
    va_list va;
    struct tm t;
    time_t epoch;
    int64_t epoch_ms;
    int remainder_ms;
    char prefix[1000]; /* MAGIC */
    char output[1000];

    /* get the time parts */
    epoch_ms = ptk_time_ms();
    epoch = (time_t)(epoch_ms / 1000);
    remainder_ms = (int)(epoch_ms % 1000);

    /* FIXME - should capture error return! */
    localtime_r(&epoch, &t);

    /* build the output string template */
    // NOLINTNEXTLINE
    snprintf(prefix, sizeof(prefix), "%04d-%02d-%02d %02d:%02d:%02d.%03d thread(%u) %s %s:%d %s\n", t.tm_year + 1900,
             t.tm_mon + 1, /* month is 0-11? */
             t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, remainder_ms, get_thread_id(),
             log_level_name[(log_level < PTK_LOG_LEVEL_NONE || log_level > PTK_LOG_LEVEL_END) ? PTK_LOG_LEVEL_END : log_level],
             func, line_num, tmpl);

    /* make sure it is zero terminated */
    prefix[sizeof(prefix) - 1] = 0;

    /* print it out. */
    va_start(va, tmpl);

    // NOLINTNEXTLINE
    vsnprintf(output, sizeof(output), prefix, va);
    fputs(output, stderr);
    fflush(stderr);

    va_end(va);
}


#define COLUMNS (16)

/**
 * @brief Internal function to log raw binary buffer data.
 *
 * @param func      Name of the calling function.
 * @param line_num  Line number in the calling source file.
 * @param log_level Log severity.
 * @param data      Buffer containing the data to log.
 */
extern void ptk_log_buf_impl(const char *func, int line_num, ptk_log_level log_level, ptk_buf *data) {
    size_t max_row, row, column;
    size_t count = 0;
    size_t old_cursor = 0;
    char row_buf[(COLUMNS * 3) + 5 + 1];

    if(!data) { return; }

    count = ptk_buf_get_cursor(data);
    old_cursor = ptk_buf_get_cursor(data);
    ptk_buf_set_cursor(data, 0);

    /* determine the number of rows we will need to print. */
    max_row = (count + (COLUMNS - 1)) / COLUMNS;

    for(row = 0; row < max_row; row++) {
        size_t offset = (row * COLUMNS);
        size_t row_offset = 0;

        /* print the offset in the packet */
        // NOLINTNEXTLINE
        row_offset = snprintf(&row_buf[0], sizeof(row_buf), "%05zu", offset);

        for(column = 0; column < COLUMNS && ((row * COLUMNS) + column) < count && row_offset < (int)sizeof(row_buf); column++) {
            uint8_t data_byte = 0;

            offset = (row * COLUMNS) + column;
            ptk_buf_get_u8(&data_byte, data, false);
            // NOLINTNEXTLINE
            row_offset += snprintf(&row_buf[row_offset], sizeof(row_buf) - (size_t)row_offset, " %02x", data_byte);
        }

        /* terminate the row string*/
        row_buf[sizeof(row_buf) - 1] = 0; /* just in case */

        /* output it, finally */
        ptk_log_impl(func, line_num, log_level, row_buf);
    }
}
