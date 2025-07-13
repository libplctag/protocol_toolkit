/**
 * @file test_ptk_buf.c
 * @brief Tests for ptk_buf API
 *
 * This file tests buffer allocation and basic usage. Logging uses ptk_log.h, not ptk_buf.h except for the functions under test.
 */
#include <ptk_buf.h>
#include <ptk_log.h>
#include <ptk_alloc.h>
#include <stdio.h>

/*
 * TODO:
 * - Add more tests for edge cases (e.g., small buffers, large buffers)
 * - Test with different data types (e.g., int, float, serializable objects)
 * - Test with invalid inputs.
 * - Test buf realloc.
 * - Test buf single u8 access (set/get). Make sure start and end indexes are updated correctly.
 * - Test serialization/deserialization results. Make sure the data is correctly serialized and deserialized.
 * - Test all ptk_buf API functions:
 *   - ptk_buf_alloc
 *   - ptk_buf_alloc_from_data
 *   - ptk_buf_realloc
 *   - ptk_serializable (serialize, deserialize)
 */

/**
 * @brief Test buffer allocation and freeing
 * @return 0 on success, nonzero on failure
 */
int test_buf_alloc() {
    info("test_buf_alloc entry");
    ptk_buf *buf = ptk_buf_alloc(64);
    if(!buf) {
        error("ptk_buf_alloc failed");
        return 1;
    }
    info("Buffer allocated, freeing");
    ptk_free(buf);
    info("test_buf_alloc exit");
    return 0;
}

int main() {
    int result = test_buf_alloc();
    if(result == 0) {
        info("ptk_buf test PASSED");
    } else {
        error("ptk_buf test FAILED");
    }
    return result;
}
