/**
 * @file test_ptk_err.c
 * @brief Tests for ptk_err API
 *
 * This file tests error code values and basic usage. Logging uses ptk_log.h, not ptk_err.h except for the functions under test.
 */
#include <ptk_err.h>
#include <ptk_log.h>
#include <stdio.h>

/*
 * Test ptk_err_set and ptk_err_get
 * - Set an error code and check if it matches
 * - Clear the error and check if it returns PTK_OK
*/

/**
 * @brief Test error code values
 * @return 0 on success, nonzero on failure
 */
int test_err_codes() {
    info("test_err_codes entry");
    if(PTK_OK != 0) {
        error("PTK_OK should be 0");
        return 1;
    }
    if(PTK_ERR_NULL_PTR == PTK_OK) {
        error("PTK_ERR_NULL_PTR should not be PTK_OK");
        return 2;
    }
    info("test_err_codes exit");
    return 0;
}

int main() {
    int result = test_err_codes();
    if(result == 0) {
        info("ptk_err test PASSED");
    } else {
        error("ptk_err test FAILED");
    }
    return result;
}
