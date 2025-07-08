#include "ptk_alloc.h"
#include "ptk_log.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Test destructor counter
static int destructor_calls = 0;

void test_destructor(void *ptr) {
    destructor_calls++;
    printf("Destructor called for %p (call #%d)\n", ptr, destructor_calls);
}

// Test default allocator
static int test_default_allocator() {
    printf("\n=== Testing Default Allocator ===\n");

    ptk_allocator_t *alloc = allocator_default_create(8);
    if(!alloc) {
        printf("FAIL: Failed to create default allocator\n");
        return 0;
    }

    // Test basic allocation
    void *ptr1 = ptk_alloc(alloc, 1024);
    if(!ptr1) {
        printf("FAIL: Basic allocation failed\n");
        ptk_allocator_destroy(alloc);
        return 0;
    }
    printf("PASS: Basic allocation successful (%p)\n", ptr1);

    // Test allocation with destructor (should work but destructor ignored)
    void *ptr2 = ptk_alloc_with_destructor(alloc, 512, test_destructor);
    if(!ptr2) {
        printf("FAIL: Allocation with destructor failed\n");
        ptk_free(alloc, ptr1);
        ptk_allocator_destroy(alloc);
        return 0;
    }
    printf("PASS: Allocation with destructor successful (%p)\n", ptr2);

    // Test reallocation
    void *ptr3 = ptk_realloc(alloc, ptr1, 2048);
    if(!ptr3) {
        printf("FAIL: Reallocation failed\n");
        ptk_free(alloc, ptr1);
        ptk_free(alloc, ptr2);
        ptk_allocator_destroy(alloc);
        return 0;
    }
    printf("PASS: Reallocation successful (%p -> %p)\n", ptr1, ptr3);

    // Clean up
    ptk_free(alloc, ptr3);
    ptk_free(alloc, ptr2);
    ptk_allocator_destroy(alloc);

    printf("PASS: Default allocator test completed\n");
    return 1;
}

// Test debug allocator
static int test_debug_allocator() {
    printf("\n=== Testing Debug Allocator ===\n");

    ptk_allocator_t *alloc = allocator_debug_create(8);
    if(!alloc) {
        printf("FAIL: Failed to create debug allocator\n");
        return 0;
    }

    // Test basic allocation
    void *ptr1 = ptk_alloc(alloc, 256);
    if(!ptr1) {
        printf("FAIL: Debug allocation failed\n");
        ptk_allocator_destroy(alloc);
        return 0;
    }
    printf("PASS: Debug allocation successful (%p)\n", ptr1);

    // Test allocation with destructor
    void *ptr2 = ptk_alloc_with_destructor(alloc, 128, test_destructor);
    if(!ptr2) {
        printf("FAIL: Debug allocation with destructor failed\n");
        ptk_free(alloc, ptr1);
        ptk_allocator_destroy(alloc);
        return 0;
    }
    printf("PASS: Debug allocation with destructor successful (%p)\n", ptr2);

    // Test statistics
    ptk_alloc_stats_t stats;
    ptk_get_stats(alloc, &stats);
    printf("PASS: Debug stats - active: %zu, total allocated: %zu\n", stats.active_allocations, stats.total_allocated);

    if(stats.active_allocations != 2) {
        printf("FAIL: Expected 2 active allocations, got %zu\n", stats.active_allocations);
        ptk_free(alloc, ptr1);
        ptk_free(alloc, ptr2);
        ptk_allocator_destroy(alloc);
        return 0;
    }

    // Test leak detection
    ptk_free(alloc, ptr1);
    if(ptk_debug_allocator_has_leaks(alloc)) {
        printf("PASS: Leak detection working (ptr2 still allocated)\n");
    } else {
        printf("FAIL: Leak detection not working\n");
        ptk_free(alloc, ptr2);
        ptk_allocator_destroy(alloc);
        return 0;
    }

    // Clean up
    ptk_free(alloc, ptr2);
    ptk_allocator_destroy(alloc);

    printf("PASS: Debug allocator test completed\n");
    return 1;
}

// Test arena allocator
static int test_arena_allocator() {
    printf("\n=== Testing Arena Allocator ===\n");

    ptk_allocator_t *alloc = allocator_arena_create(4096, 8);
    if(!alloc) {
        printf("FAIL: Failed to create arena allocator\n");
        return 0;
    }

    // Reset destructor counter
    destructor_calls = 0;

    // Test basic allocation
    void *ptr1 = ptk_alloc(alloc, 512);
    if(!ptr1) {
        printf("FAIL: Arena allocation failed\n");
        ptk_allocator_destroy(alloc);
        return 0;
    }
    printf("PASS: Arena allocation successful (%p)\n", ptr1);

    // Test allocation with destructor
    void *ptr2 = ptk_alloc_with_destructor(alloc, 256, test_destructor);
    if(!ptr2) {
        printf("FAIL: Arena allocation with destructor failed\n");
        ptk_allocator_destroy(alloc);
        return 0;
    }
    printf("PASS: Arena allocation with destructor successful (%p)\n", ptr2);

    // Test another allocation with destructor
    void *ptr3 = ptk_alloc_with_destructor(alloc, 128, test_destructor);
    if(!ptr3) {
        printf("FAIL: Second arena allocation with destructor failed\n");
        ptk_allocator_destroy(alloc);
        return 0;
    }
    printf("PASS: Second arena allocation with destructor successful (%p)\n", ptr3);

    // Test statistics
    ptk_alloc_stats_t stats;
    ptk_get_stats(alloc, &stats);
    printf("PASS: Arena stats - active: %zu, total allocated: %zu\n", stats.active_allocations, stats.total_allocated);

    // Test arena reset
    printf("Testing arena reset...\n");
    ptk_reset(alloc);
    ptk_get_stats(alloc, &stats);
    if(stats.total_allocated != 0 || stats.active_allocations != 0) {
        printf("FAIL: Arena reset didn't clear stats properly\n");
        ptk_allocator_destroy(alloc);
        return 0;
    }
    printf("PASS: Arena reset successful\n");

    // Allocate again after reset
    void *ptr4 = ptk_alloc_with_destructor(alloc, 64, test_destructor);
    if(!ptr4) {
        printf("FAIL: Allocation after arena reset failed\n");
        ptk_allocator_destroy(alloc);
        return 0;
    }
    printf("PASS: Allocation after reset successful (%p)\n", ptr4);

    // Destroy allocator (should call remaining destructors)
    printf("Destroying arena allocator (should call destructors)...\n");
    ptk_allocator_destroy(alloc);

    // Check if destructors were called
    if(destructor_calls != 1) {
        printf("FAIL: Expected 1 destructor call, got %d\n", destructor_calls);
        return 0;
    }
    printf("PASS: Destructor called correctly during arena cleanup\n");

    printf("PASS: Arena allocator test completed\n");
    return 1;
}

// Test allocator alignment
static int test_allocator_alignment() {
    printf("\n=== Testing Allocator Alignment ===\n");

    ptk_allocator_t *alloc = allocator_default_create(16);
    if(!alloc) {
        printf("FAIL: Failed to create allocator with 16-byte alignment\n");
        return 0;
    }

    void *ptr = ptk_alloc(alloc, 17);  // Odd size to test alignment
    if(!ptr) {
        printf("FAIL: Aligned allocation failed\n");
        ptk_allocator_destroy(alloc);
        return 0;
    }

    uintptr_t addr = (uintptr_t)ptr;
    if(addr % 16 != 0) {
        printf("FAIL: Allocation not properly aligned (addr = 0x%lx)\n", addr);
        ptk_free(alloc, ptr);
        ptk_allocator_destroy(alloc);
        return 0;
    }

    printf("PASS: Allocation properly aligned to 16 bytes (addr = 0x%lx)\n", addr);

    ptk_free(alloc, ptr);
    ptk_allocator_destroy(alloc);

    printf("PASS: Alignment test completed\n");
    return 1;
}

int main() {
    printf("=== Protocol Toolkit Allocator Tests ===\n");

    int tests_passed = 0;
    int total_tests = 4;

    if(test_default_allocator()) { tests_passed++; }
    if(test_debug_allocator()) { tests_passed++; }
    if(test_arena_allocator()) { tests_passed++; }
    if(test_allocator_alignment()) { tests_passed++; }

    printf("\n=== Test Results ===\n");
    printf("Passed: %d/%d tests\n", tests_passed, total_tests);

    if(tests_passed == total_tests) {
        printf("✓ All allocator tests passed!\n");
        return 0;
    } else {
        printf("✗ Some tests failed!\n");
        return 1;
    }
}
