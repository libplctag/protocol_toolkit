#include "ptk_alloc.h"
#include "ptk_buf.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Test basic buffer operations
static int test_buffer_basic() {
    printf("\n=== Testing Basic Buffer Operations ===\n");

    ptk_allocator_t *alloc = allocator_default_create(8);
    if(!alloc) {
        printf("FAIL: Failed to create allocator\n");
        return 0;
    }

    ptk_buf_t *buf = ptk_buf_create(alloc, 1024);
    if(!buf) {
        printf("FAIL: Buffer creation failed\n");
        ptk_allocator_destroy(alloc);
        return 0;
    }
    printf("PASS: Buffer creation successful\n");

    // Test buffer properties
    if(ptk_buf_cap(buf) != 1024) {
        printf("FAIL: Expected capacity 1024, got %zu\n", ptk_buf_cap(buf));
        ptk_buf_dispose(buf);
        ptk_allocator_destroy(alloc);
        return 0;
    }
    printf("PASS: Buffer capacity correct\n");

    if(ptk_buf_len(buf) != 0) {
        printf("FAIL: Expected initial length 0, got %zu\n", ptk_buf_len(buf));
        ptk_buf_dispose(buf);
        ptk_allocator_destroy(alloc);
        return 0;
    }
    printf("PASS: Buffer initial length correct\n");

    // Test basic positioning
    if(ptk_buf_get_start(buf) != 0) {
        printf("FAIL: Expected start position 0, got %zu\n", ptk_buf_get_start(buf));
        ptk_buf_dispose(buf);
        ptk_allocator_destroy(alloc);
        return 0;
    }
    printf("PASS: Buffer start position correct\n");

    if(ptk_buf_get_end(buf) != 0) {
        printf("FAIL: Expected end position 0, got %zu\n", ptk_buf_get_end(buf));
        ptk_buf_dispose(buf);
        ptk_allocator_destroy(alloc);
        return 0;
    }
    printf("PASS: Buffer end position correct\n");

    // Test setting end position (simulating data write)
    if(ptk_buf_set_end(buf, 13) != PTK_OK) {
        printf("FAIL: Setting end position failed\n");
        ptk_buf_dispose(buf);
        ptk_allocator_destroy(alloc);
        return 0;
    }

    if(ptk_buf_len(buf) != 13) {
        printf("FAIL: Expected length 13 after setting end, got %zu\n", ptk_buf_len(buf));
        ptk_buf_dispose(buf);
        ptk_allocator_destroy(alloc);
        return 0;
    }
    printf("PASS: Buffer length correct after setting end position\n");

    ptk_buf_dispose(buf);
    ptk_allocator_destroy(alloc);

    printf("PASS: Basic buffer test completed\n");
    return 1;
}

// Test buffer positioning
static int test_buffer_positioning() {
    printf("\n=== Testing Buffer Positioning ===\n");

    ptk_allocator_t *alloc = allocator_default_create(8);
    if(!alloc) {
        printf("FAIL: Failed to create allocator\n");
        return 0;
    }

    ptk_buf_t *buf = ptk_buf_create(alloc, 100);
    if(!buf) {
        printf("FAIL: Buffer creation failed\n");
        ptk_allocator_destroy(alloc);
        return 0;
    }

    // Set some data in buffer
    if(ptk_buf_set_end(buf, 50) != PTK_OK) {
        printf("FAIL: Setting end position failed\n");
        ptk_buf_dispose(buf);
        ptk_allocator_destroy(alloc);
        return 0;
    }

    // Test moving start position
    if(ptk_buf_set_start(buf, 10) != PTK_OK) {
        printf("FAIL: Setting start position failed\n");
        ptk_buf_dispose(buf);
        ptk_allocator_destroy(alloc);
        return 0;
    }

    if(ptk_buf_len(buf) != 40) {  // 50 - 10 = 40
        printf("FAIL: Expected length 40 after repositioning, got %zu\n", ptk_buf_len(buf));
        ptk_buf_dispose(buf);
        ptk_allocator_destroy(alloc);
        return 0;
    }
    printf("PASS: Buffer length correct after repositioning\n");

    // Test remaining capacity
    size_t remaining = ptk_buf_get_remaining(buf);
    if(remaining != 50) {  // 100 - 50 = 50
        printf("FAIL: Expected remaining capacity 50, got %zu\n", remaining);
        ptk_buf_dispose(buf);
        ptk_allocator_destroy(alloc);
        return 0;
    }
    printf("PASS: Remaining capacity correct\n");

    // Test error conditions
    if(ptk_buf_set_start(buf, 60) == PTK_OK) {  // start > end should fail
        printf("FAIL: Setting start beyond end should have failed\n");
        ptk_buf_dispose(buf);
        ptk_allocator_destroy(alloc);
        return 0;
    }
    printf("PASS: Error handling for invalid start position works\n");

    if(ptk_buf_set_end(buf, 150) == PTK_OK) {  // end > capacity should fail
        printf("FAIL: Setting end beyond capacity should have failed\n");
        ptk_buf_dispose(buf);
        ptk_allocator_destroy(alloc);
        return 0;
    }
    printf("PASS: Error handling for invalid end position works\n");

    ptk_buf_dispose(buf);
    ptk_allocator_destroy(alloc);

    printf("PASS: Buffer positioning test completed\n");
    return 1;
}

int main() {
    printf("=== Protocol Toolkit Buffer Tests ===\n");

    int tests_passed = 0;
    int total_tests = 2;

    if(test_buffer_basic()) { tests_passed++; }
    if(test_buffer_positioning()) { tests_passed++; }

    printf("\n=== Test Results ===\n");
    printf("Passed: %d/%d tests\n", tests_passed, total_tests);

    if(tests_passed == total_tests) {
        printf("✓ All buffer tests passed!\n");
        return 0;
    } else {
        printf("✗ Some tests failed!\n");
        return 1;
    }
}
