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
#include <stdlib.h>
#include <string.h>

/*
 * TODO:
 * - Test all ptk_alloc API functions:
 *   - ptk_alloc
 *   - ptk_alloc_impl
 *   - ptk_realloc
 *   - ptk_realloc_impl
 *   - ptk_free
 *   - ptk_free_impl
 *   - Canary validation for malloc() pointer detection
 *   - Memory corruption detection
 *   - Double-free detection
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
    ptk_free(&obj);
    if(!destructor_called) {
        error("test_destructor was NOT called");
        return 2;
    }
    if(obj != NULL) {
        error("Pointer was not set to NULL after ptk_free");
        return 3;
    }
    info("test_alloc_free exit");
    return 0;
}

/**
 * @brief Test malloc() pointer detection via canary validation
 * @return 0 on success, nonzero on failure
 */
int test_malloc_detection() {
    info("test_malloc_detection entry");
    
    // Allocate memory with malloc() instead of ptk_alloc()
    void *malloc_ptr = malloc(128);
    if(!malloc_ptr) {
        error("malloc failed");
        return 1;
    }
    
    info("Testing ptk_free with malloc() pointer - should fail gracefully");
    
    // This should fail and not crash
    ptk_err old_err = ptk_get_err();
    ptk_set_err(PTK_OK);  // Clear any existing error
    
    ptk_free(&malloc_ptr);
    
    // Check that an error was set
    ptk_err new_err = ptk_get_err();
    if(new_err == PTK_OK) {
        error("ptk_free should have detected malloc() pointer and set an error");
        free(malloc_ptr);  // Clean up
        return 2;
    }
    
    info("ptk_free correctly detected malloc() pointer and set error: %d", new_err);
    
    // malloc_ptr should still be valid since ptk_free should have refused to free it
    if(malloc_ptr == NULL) {
        error("ptk_free should not have modified malloc() pointer");
        return 3;
    }
    
    // Clean up with regular free
    free(malloc_ptr);
    
    info("test_malloc_detection exit");
    return 0;
}

/**
 * @brief Test realloc with canary validation
 * @return 0 on success, nonzero on failure
 */
int test_realloc_validation() {
    info("test_realloc_validation entry");
    
    // Allocate with ptk_alloc
    void *ptr = ptk_alloc(64, NULL);
    if(!ptr) {
        error("ptk_alloc failed");
        return 1;
    }
    
    // Write some test data
    strcpy((char*)ptr, "Hello, World!");
    
    // Reallocate to larger size
    void *new_ptr = ptk_realloc(ptr, 128);
    if(!new_ptr) {
        error("ptk_realloc failed");
        ptk_free(&ptr);
        return 2;
    }
    
    // Check that data was preserved
    if(strcmp((char*)new_ptr, "Hello, World!") != 0) {
        error("Data not preserved during realloc");
        ptk_free(&new_ptr);
        return 3;
    }
    
    // Test realloc with malloc() pointer
    void *malloc_ptr = malloc(64);
    if(!malloc_ptr) {
        error("malloc failed");
        ptk_free(&new_ptr);
        return 4;
    }
    
    info("Testing ptk_realloc with malloc() pointer - should fail");
    ptk_set_err(PTK_OK);  // Clear error
    
    void *bad_realloc = ptk_realloc(malloc_ptr, 128);
    if(bad_realloc != NULL) {
        error("ptk_realloc should have failed with malloc() pointer");
        free(malloc_ptr);
        ptk_free(&new_ptr);
        return 5;
    }
    
    ptk_err err = ptk_get_err();
    if(err == PTK_OK) {
        error("ptk_realloc should have set an error for malloc() pointer");
        free(malloc_ptr);
        ptk_free(&new_ptr);
        return 6;
    }
    
    info("ptk_realloc correctly rejected malloc() pointer with error: %d", err);
    
    // Clean up
    free(malloc_ptr);
    ptk_free(&new_ptr);
    
    info("test_realloc_validation exit");
    return 0;
}

/**
 * @brief Test double-free detection
 * @return 0 on success, nonzero on failure
 */
int test_double_free_detection() {
    info("test_double_free_detection entry");
    
    void *ptr = ptk_alloc(32, NULL);
    if(!ptr) {
        error("ptk_alloc failed");
        return 1;
    }
    
    // First free - should succeed
    ptk_set_err(PTK_OK);
    ptk_free(&ptr);
    
    if(ptk_get_err() != PTK_OK) {
        error("First ptk_free failed unexpectedly");
        return 2;
    }
    
    if(ptr != NULL) {
        error("Pointer not nulled after first free");
        return 3;
    }
    
    // Second free - should be safe (just debug message for NULL pointer)
    info("Testing double-free with NULL pointer - should be safe");
    ptk_set_err(PTK_OK);
    ptk_free(&ptr);  // ptr is already NULL
    
    if(ptk_get_err() != PTK_OK) {
        error("Double-free of NULL pointer should be safe");
        return 4;
    }
    
    info("test_double_free_detection exit");
    return 0;
}

/**
 * @brief Test memory corruption detection
 * @return 0 on success, nonzero on failure
 */
int test_memory_corruption_detection() {
    info("test_memory_corruption_detection entry");
    
    void *ptr = ptk_alloc(64, NULL);
    if(!ptr) {
        error("ptk_alloc failed");
        return 1;
    }
    
    // Deliberately corrupt the footer canary
    // This simulates buffer overflow
    info("Simulating buffer overflow by corrupting footer canary");
    
    // Calculate where the footer should be
    // This is implementation-dependent and fragile, but needed for testing
    uint64_t *footer_location = (uint64_t*)((char*)ptr + 64);  // 64 is the allocated size, rounded up to 16-byte boundary
    
    // Corrupt the footer canary
    *footer_location = 0xBADBADBADBADBADULL;
    
    // Now try to free - should detect corruption
    ptk_set_err(PTK_OK);
    ptk_free(&ptr);
    
    ptk_err err = ptk_get_err();
    if(err == PTK_OK) {
        error("ptk_free should have detected footer corruption");
        // If it didn't detect corruption, ptr might be freed, don't access it
        return 2;
    }
    
    info("ptk_free correctly detected memory corruption with error: %d", err);
    
    // Since ptk_free refused to free corrupted memory, we have a leak
    // In a real scenario, this would be logged and handled appropriately
    warn("Memory leak due to corruption detection - this is expected behavior");
    
    info("test_memory_corruption_detection exit");
    return 0;
}

/**
 * @brief Test header corruption detection
 * @return 0 on success, nonzero on failure
 */
int test_header_corruption_detection() {
    info("test_header_corruption_detection entry");
    
    void *ptr = ptk_alloc(32, NULL);
    if(!ptr) {
        error("ptk_alloc failed");
        return 1;
    }
    
    info("Simulating header corruption by modifying header canary");
    
    // Access the header to corrupt the canary
    // This is implementation-dependent but necessary for testing
    typedef struct {
        uint64_t header_canary;
        void (*destructor)(void *ptr);
        size_t size;
        const char *file;
        int line;
    } test_header_t;
    
    test_header_t *header = (test_header_t*)((char*)ptr - sizeof(test_header_t));
    uint64_t original_canary = header->header_canary;
    
    // Corrupt the header canary
    header->header_canary = 0xDEADDEADDEADDEADULL;
    
    // Try to free - should detect header corruption
    ptk_set_err(PTK_OK);
    ptk_free(&ptr);
    
    ptk_err err = ptk_get_err();
    if(err == PTK_OK) {
        error("ptk_free should have detected header corruption");
        return 2;
    }
    
    info("ptk_free correctly detected header corruption with error: %d", err);
    
    // Restore canary to prevent further issues (though memory is likely leaked)
    header->header_canary = original_canary;
    
    warn("Memory leak due to header corruption detection - this is expected behavior");
    
    info("test_header_corruption_detection exit");
    return 0;
}

/**
 * @brief Test comprehensive canary validation scenarios
 * @return 0 on success, nonzero on failure
 */
int test_comprehensive_canary_validation() {
    info("test_comprehensive_canary_validation entry");
    
    // Test 1: Verify canary constants are as expected
    info("Verifying canary constants are properly defined");
    
    void *test_ptr = ptk_alloc(16, NULL);
    if(!test_ptr) {
        error("ptk_alloc failed for canary validation test");
        return 1;
    }
    
    // Write some data to ensure we don't accidentally hit the canaries
    strcpy((char*)test_ptr, "test");
    
    // Free normally to verify canaries work
    ptk_set_err(PTK_OK);
    ptk_free(&test_ptr);
    
    if(ptk_get_err() != PTK_OK) {
        error("Normal free failed canary validation - implementation issue");
        return 2;
    }
    
    info("Basic canary validation working correctly");
    
    // Test 2: Verify malloc detection works with different sizes
    info("Testing malloc detection with various allocation sizes");
    
    size_t test_sizes[] = {1, 8, 16, 32, 64, 128, 256, 1024};
    int num_sizes = sizeof(test_sizes) / sizeof(test_sizes[0]);
    
    for(int i = 0; i < num_sizes; i++) {
        void *malloc_ptr = malloc(test_sizes[i]);
        if(!malloc_ptr) {
            warn("malloc failed for size %zu, skipping", test_sizes[i]);
            continue;
        }
        
        ptk_set_err(PTK_OK);
        ptk_free(&malloc_ptr);
        
        if(ptk_get_err() == PTK_OK) {
            error("malloc detection failed for size %zu", test_sizes[i]);
            free(malloc_ptr);
            return 3;
        }
        
        if(malloc_ptr == NULL) {
            error("ptk_free modified malloc pointer for size %zu", test_sizes[i]);
            return 4;
        }
        
        free(malloc_ptr);
    }
    
    info("malloc detection working correctly for all test sizes");
    
    // Test 3: Verify edge case handling
    info("Testing edge cases and boundary conditions");
    
    // Very small allocation
    void *tiny_ptr = ptk_alloc(1, NULL);
    if(!tiny_ptr) {
        error("Failed to allocate 1 byte");
        return 5;
    }
    
    *(char*)tiny_ptr = 'X';  // Write to the single byte
    
    ptk_set_err(PTK_OK);
    ptk_free(&tiny_ptr);
    
    if(ptk_get_err() != PTK_OK || tiny_ptr != NULL) {
        error("Small allocation free failed validation");
        return 6;
    }
    
    // Large allocation
    void *large_ptr = ptk_alloc(4096, NULL);
    if(!large_ptr) {
        warn("Failed to allocate 4096 bytes, skipping large allocation test");
    } else {
        memset(large_ptr, 0xAA, 4096);  // Fill with pattern
        
        ptk_set_err(PTK_OK);
        ptk_free(&large_ptr);
        
        if(ptk_get_err() != PTK_OK || large_ptr != NULL) {
            error("Large allocation free failed validation");
            return 7;
        }
        
        info("Large allocation canary validation working correctly");
    }
    
    info("test_comprehensive_canary_validation exit");
    return 0;
}

int main() {
    int total_failures = 0;
    
    info("=== Starting PTK Allocation Tests ===");
    
    // Test 1: Basic allocation and freeing
    info("\n--- Test 1: Basic Allocation/Free ---");
    int result1 = test_alloc_free();
    if(result1 == 0) {
        info("✓ Basic allocation/free test PASSED");
    } else {
        error("✗ Basic allocation/free test FAILED with code %d", result1);
        total_failures++;
    }
    
    // Test 2: malloc() pointer detection
    info("\n--- Test 2: malloc() Pointer Detection ---");
    int result2 = test_malloc_detection();
    if(result2 == 0) {
        info("✓ malloc() pointer detection test PASSED");
    } else {
        error("✗ malloc() pointer detection test FAILED with code %d", result2);
        total_failures++;
    }
    
    // Test 3: Realloc validation
    info("\n--- Test 3: Realloc Validation ---");
    int result3 = test_realloc_validation();
    if(result3 == 0) {
        info("✓ Realloc validation test PASSED");
    } else {
        error("✗ Realloc validation test FAILED with code %d", result3);
        total_failures++;
    }
    
    // Test 4: Double-free detection
    info("\n--- Test 4: Double-Free Detection ---");
    int result4 = test_double_free_detection();
    if(result4 == 0) {
        info("✓ Double-free detection test PASSED");
    } else {
        error("✗ Double-free detection test FAILED with code %d", result4);
        total_failures++;
    }
    
    // Test 5: Memory corruption detection
    info("\n--- Test 5: Memory Corruption Detection ---");
    int result5 = test_memory_corruption_detection();
    if(result5 == 0) {
        info("✓ Memory corruption detection test PASSED");
    } else {
        error("✗ Memory corruption detection test FAILED with code %d", result5);
        total_failures++;
    }
    
    // Test 6: Header corruption detection
    info("\n--- Test 6: Header Corruption Detection ---");
    int result6 = test_header_corruption_detection();
    if(result6 == 0) {
        info("✓ Header corruption detection test PASSED");
    } else {
        error("✗ Header corruption detection test FAILED with code %d", result6);
        total_failures++;
    }
    
    // Test 7: Comprehensive canary validation
    info("\n--- Test 7: Comprehensive Canary Validation ---");
    int result7 = test_comprehensive_canary_validation();
    if(result7 == 0) {
        info("✓ Comprehensive canary validation test PASSED");
    } else {
        error("✗ Comprehensive canary validation test FAILED with code %d", result7);
        total_failures++;
    }
    
    // Summary
    info("\n=== PTK Allocation Test Summary ===");
    if(total_failures == 0) {
        info("🎉 ALL TESTS PASSED (7/7)");
        info("Canary protection is working correctly!");
    } else {
        error("❌ %d out of 7 tests FAILED", total_failures);
    }
    
    return total_failures;
}
