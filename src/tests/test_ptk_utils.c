/**
 * @file test_ptk_utils.c
 * @brief Tests for ptk_utils API
 *
 * This file tests timekeeping macros and types. Logging uses ptk_log.h, not ptk_utils.h except for the functions under test.
 */
#include <ptk_utils.h>
#include <ptk_log.h>
#include <stdio.h>

/**
 * @brief Test time macros
 * @return 0 on success, nonzero on failure
 */
int test_time_macros() {
    info("test_time_macros entry");
    if(PTK_TIME_WAIT_FOREVER <= 0) {
        error("PTK_TIME_WAIT_FOREVER should be positive");
        return 1;
    }
    if(PTK_TIME_NO_WAIT >= 0) {
        error("PTK_TIME_NO_WAIT should be negative");
        return 2;
    }
    info("test_time_macros exit");
    return 0;
}

int main() {
    int result = test_time_macros();
    if(result == 0) {
        info("ptk_utils test PASSED");
    } else {
        error("ptk_utils test FAILED");
    }
    return result;
}
