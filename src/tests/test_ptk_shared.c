/**
 * @file test_ptk_shared.c
 * @brief Tests for ptk_shared API
 *
 * This file tests shared memory handle creation and usage. Logging uses ptk_log.h, not ptk_shared.h except for the functions under test.
 */
#include <ptk_mem.h>
#include <ptk_log.h>
#include <stdio.h>

/*
 * TODO:
 * - Use multiple threads to test concurrent access
 * - Test with different data types
 * - Validate that handles are released correctly
 * - Test error handling (e.g., invalid handles)
 * - Test with a ptk_alloc destructor to make sure that shared memory is cleaned up correctly
 * - Test all ptk_shared API functions and macros:
 *   - ptk_shared_wrap
 *   - ptk_shared_wrap_impl
 *   - ptk_shared_init
 *   - ptk_shared_shutdown
 *   - ptk_shared_acquire
 *   - ptk_shared_realloc
 *   - ptk_shared_release
 *   - PTK_SHARED_INVALID_HANDLE
 *   - PTK_SHARED_IS_VALID
 *   - PTK_SHARED_HANDLE_EQUAL
 */

/**
 * @brief Test shared memory handle creation and acquire/release
 * @return 0 on success, nonzero on failure
 */
int test_shared_handle() {
    info("test_shared_handle entry");
    
    // Initialize shared memory system
    ptk_err err = ptk_shared_init();
    if (err != PTK_OK) {
        error("ptk_shared_init failed");
        return 1;
    }
    
    // Allocate shared memory directly
    ptk_shared_handle_t handle = ptk_shared_alloc(sizeof(int), NULL);
    if(!PTK_SHARED_IS_VALID(handle)) {
        error("ptk_shared_alloc failed");
        ptk_shared_shutdown();
        return 2;
    }
    
    int *acquired = (int*)ptk_shared_acquire(handle, PTK_TIME_WAIT_FOREVER);
    if(!acquired) {
        error("ptk_shared_acquire failed");
        ptk_shared_release(handle);
        ptk_shared_shutdown();
        return 3;
    }
    *acquired = 42;
    ptk_shared_release(handle);
    
    // Clean up
    ptk_shared_release(handle);
    ptk_shared_shutdown();
    
    info("test_shared_handle exit");
    return 0;
}

int main() {
    int result = test_shared_handle();
    if(result == 0) {
        info("ptk_shared test PASSED");
    } else {
        error("ptk_shared test FAILED");
    }
    return result;
}
