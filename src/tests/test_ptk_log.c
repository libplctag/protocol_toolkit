/**
 * @file test_ptk_log.c
 * @brief Tests for ptk_log API
 *
 * This file tests logging macros. Logging uses ptk_log.h, not ptk_log.h except for the macros under test.
 */
#include <ptk_log.h>
#include <stdio.h>

/*
 * TODO:
 * - Test with different log levels (e.g., info, warn, error)
 * - Validate that the logs are being made and are in the right format by redirecting output to a file or buffer.
 * - Test all ptk_log API functions and macros:
 *   - ptk_log_level_set
 *   - ptk_log_level_get
 *   - ptk_log_impl
 *   - info
 *   - warn
 *   - error
 *   - debug
 *   - trace
 */

/**
 * @brief Test logging macros
 * @return 0 on success, nonzero on failure
 */
int test_log_macros() {
    info("test_log_macros entry");
    info("This is an info log");
    warn("This is a warning log");
    error("This is an error log");
    debug("This is a debug log");
    trace("This is a trace log");
    info("test_log_macros exit");
    return 0;
}

int main() {
    int result = test_log_macros();
    if(result == 0) {
        info("ptk_log test PASSED");
    } else {
        error("ptk_log test FAILED");
    }
    return result;
}
