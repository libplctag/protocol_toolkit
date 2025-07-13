/**
 * @file test_ptk_alloc.c
 * @brief Tests for ptk_alloc and ptk_free APIs
 *
 * This file tests the memory allocation and deallocation functions of the Protocol Toolkit.
 * Logging and error handling use APIs from ptk_log.h and ptk_err.h, which are NOT under test here.
 */

#include <ptk_alloc.h>
#include <ptk_log.h>
#include <ptk_err.h>
#include <stdio.h>

/*
 * TODO:
 * - Test all ptk_alloc API functions:
 *   - ptk_alloc
 *   - ptk_alloc_impl
 *   - ptk_realloc
 *   - ptk_realloc_impl
 *   - ptk_free
 *   - ptk_free_impl
 */

/**
 * @brief Simple destructor for test object
 * @param ptr Pointer to object being destroyed
 */
static int destructor_called = 0;
static void test_destructor(void *ptr) {
    info("test_destructor called");
    destructor_called = 1;
    // ...additional cleanup if needed...
}

/**
 * @brief Test allocation and freeing of memory
 * @return 0 on success, nonzero on failure
 */

int test_alloc_free() {
    info("test_alloc_free entry");
    destructor_called = 0;
    void *obj = ptk_alloc(128, test_destructor);
    if(!obj) {
        error("ptk_alloc failed");
        return 1;
    }
    info("Allocated memory, now freeing");
    ptk_free(obj);
    if(!destructor_called) {
        error("test_destructor was NOT called");
        return 2;
    }
    info("test_alloc_free exit");
    return 0;
}

int main() {
    int result = test_alloc_free();
    if(result == 0) {
        info("ptk_alloc test PASSED");
    } else {
        error("ptk_alloc test FAILED");
    }
    return result;
}
