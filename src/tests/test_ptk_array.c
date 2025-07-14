
/**
 * @file test_ptk_array.c
 * @brief Tests for ptk_array API
 *
 * This file tests the type-safe array macros and functions as designed in ptk_array.h.
 * Logging uses ptk_log.h, not ptk_array.h except for the functions under test.
 */

/*
 * TODO:
 * - Add more tests for edge cases (empty arrays, resizing, etc.)
 * - Test with different element types
 * - Validate memory management (destructors called, no leaks)
 * - Test error handling (invalid indices, null pointers)
 * - Test all ptk_array API macros and functions:
 *   - PTK_ARRAY_DECLARE
 *   - <PREFIX>_array_create
 *   - <PREFIX>_array_resize
 *   - <PREFIX>_array_append
 *   - <PREFIX>_array_get
 *   - <PREFIX>_array_set
 *   - <PREFIX>_array_copy
 *   - <PREFIX>_array_is_valid
 *   - <PREFIX>_array_from_raw
 *   - <PREFIX>_array_len
*/ 
#include <ptk_array.h>
#include <ptk_log.h>
#include <ptk_mem.h>
#include <stdio.h>

typedef struct { int x; } test_elem_t;
static void test_elem_destructor(test_elem_t *elem) {
    info("test_elem_destructor called for x=%d", elem->x);
}

PTK_ARRAY_DECLARE(test_elem, test_elem_t)

int test_array_ops() {
    info("test_array_ops entry");
    test_elem_array_t *arr = test_elem_array_create(2, test_elem_destructor);
    if(!arr) {
        error("array create failed");
        return 1;
    }
    arr->elements[0].x = 10;
    arr->elements[1].x = 20;
    if(test_elem_array_resize(arr, 4) != PTK_OK) {
        error("array resize failed");
        ptk_local_free(arr);
        return 2;
    }
    if(test_elem_array_append(arr, (test_elem_t){.x=30}) != PTK_OK) {
        error("array append failed");
        ptk_local_free(arr);
        return 3;
    }
    test_elem_t val;
    if(test_elem_array_get(arr, 2, &val) != PTK_OK || val.x != 30) {
        error("array get failed or value mismatch");
        ptk_local_free(arr);
        return 4;
    }
    if(test_elem_array_set(arr, 1, (test_elem_t){.x=99}) != PTK_OK || arr->elements[1].x != 99) {
        error("array set failed or value mismatch");
        ptk_local_free(arr);
        return 5;
    }
    test_elem_array_t *copy = test_elem_array_copy(arr);
    if(!copy || copy->len != arr->len || copy->elements[1].x != 99) {
        error("array copy failed or mismatch");
        ptk_local_free(arr);
        ptk_local_free(copy);
        return 6;
    }
    ptk_local_free(arr);
    ptk_local_free(copy);
    info("test_array_ops exit");
    return 0;
}

int main() {
    int result = test_array_ops();
    if(result == 0) {
        info("ptk_array test PASSED");
    } else {
        error("ptk_array test FAILED");
    }
    return result;
}
