/**
 * @file test_ptk_array_comprehensive.c
 * @brief Comprehensive tests for ptk_array.h API
 *
 * Tests all array operations including creation, resize, append, get, set,
 * copy, validation, and raw array creation functionality.
 */

#include <ptk_array.h>
#include <ptk_log.h>
#include <ptk_err.h>
#include <ptk_mem.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

//=============================================================================
// Test Data Types and Destructors
//=============================================================================

typedef struct {
    int value;
    char *name;
} test_struct_t;

static void test_struct_destructor(test_struct_t *element) {
    if (element && element->name) {
        ptk_local_free(&element->name);
        element->name = NULL;
    }
}

// Declare array types for testing
PTK_ARRAY_DECLARE(int, int)
PTK_ARRAY_DECLARE(test_struct, test_struct_t)
PTK_ARRAY_DECLARE(double, double)

//=============================================================================
// Basic Array Operations Tests
//=============================================================================

int test_array_creation_and_validation(void) {
    info("test_array_creation_and_validation entry");
    
    // Test int_array_create
    int_array_t *int_arr = int_array_create(5, NULL);
    if (!int_arr) {
        error("int_array_create failed");
        return 1;
    }
    
    // Test int_array_is_valid
    if (!int_array_is_valid(int_arr)) {
        error("int_array_is_valid returned false for valid array");
        ptk_local_free(&int_arr);
        return 2;
    }
    
    // Test int_array_len
    if (int_array_len(int_arr) != 5) {
        error("int_array_len returned wrong length: %zu != 5", int_array_len(int_arr));
        ptk_local_free(&int_arr);
        return 3;
    }
    
    ptk_local_free(&int_arr);
    
    // Test creation with zero size (should return NULL)
    int_array_t *zero_arr = int_array_create(0, NULL);
    if (zero_arr != NULL) {
        error("int_array_create should return NULL for zero size");
        ptk_local_free(&zero_arr);
        return 4;
    }
    
    // Test validation with NULL array
    if (int_array_is_valid(NULL)) {
        error("int_array_is_valid should return false for NULL array");
        return 5;
    }
    
    // Test length with NULL array
    if (int_array_len(NULL) != 0) {
        error("int_array_len should return 0 for NULL array");
        return 6;
    }
    
    info("test_array_creation_and_validation exit");
    return 0;
}

int test_array_get_set_operations(void) {
    info("test_array_get_set_operations entry");
    
    int_array_t *arr = int_array_create(3, NULL);
    if (!arr) {
        error("int_array_create failed");
        return 1;
    }
    
    // Test int_array_set
    for (int i = 0; i < 3; i++) {
        ptk_err result = int_array_set(arr, i, i * 10);
        if (result != PTK_OK) {
            error("int_array_set failed at index %d", i);
            ptk_local_free(&arr);
            return 2;
        }
    }
    
    // Test int_array_get
    for (int i = 0; i < 3; i++) {
        int value;
        ptk_err result = int_array_get(arr, i, &value);
        if (result != PTK_OK) {
            error("int_array_get failed at index %d", i);
            ptk_local_free(&arr);
            return 3;
        }
        
        if (value != i * 10) {
            error("int_array_get returned wrong value at index %d: %d != %d", i, value, i * 10);
            ptk_local_free(&arr);
            return 4;
        }
    }
    
    // Test out-of-bounds access
    int dummy;
    ptk_err result = int_array_get(arr, 10, &dummy);
    if (result != PTK_ERR_OUT_OF_BOUNDS) {
        error("int_array_get should return PTK_ERR_OUT_OF_BOUNDS for out-of-bounds access");
        ptk_local_free(&arr);
        return 5;
    }
    
    result = int_array_set(arr, 10, 42);
    if (result != PTK_ERR_OUT_OF_BOUNDS) {
        error("int_array_set should return PTK_ERR_OUT_OF_BOUNDS for out-of-bounds access");
        ptk_local_free(&arr);
        return 6;
    }
    
    // Test NULL parameter handling
    result = int_array_get(NULL, 0, &dummy);
    if (result != PTK_ERR_NULL_PTR) {
        error("int_array_get should return PTK_ERR_NULL_PTR for NULL array");
        ptk_local_free(&arr);
        return 7;
    }
    
    result = int_array_get(arr, 0, NULL);
    if (result != PTK_ERR_NULL_PTR) {
        error("int_array_get should return PTK_ERR_NULL_PTR for NULL element");
        ptk_local_free(&arr);
        return 8;
    }
    
    result = int_array_set(NULL, 0, 42);
    if (result != PTK_ERR_NULL_PTR) {
        error("int_array_set should return PTK_ERR_NULL_PTR for NULL array");
        ptk_local_free(&arr);
        return 9;
    }
    
    ptk_local_free(&arr);
    
    info("test_array_get_set_operations exit");
    return 0;
}

int test_array_resize_operations(void) {
    info("test_array_resize_operations entry");
    
    int_array_t *arr = int_array_create(3, NULL);
    if (!arr) {
        error("int_array_create failed");
        return 1;
    }
    
    // Initialize with test values
    for (int i = 0; i < 3; i++) {
        int_array_set(arr, i, i + 100);
    }
    
    // Test expanding the array
    ptk_err result = int_array_resize(arr, 5);
    if (result != PTK_OK) {
        error("int_array_resize (expand) failed");
        ptk_local_free(&arr);
        return 2;
    }
    
    if (int_array_len(arr) != 5) {
        error("Array length after resize incorrect: %zu != 5", int_array_len(arr));
        ptk_local_free(&arr);
        return 3;
    }
    
    // Verify old values are preserved
    for (int i = 0; i < 3; i++) {
        int value;
        int_array_get(arr, i, &value);
        if (value != i + 100) {
            error("Value at index %d changed after resize: %d != %d", i, value, i + 100);
            ptk_local_free(&arr);
            return 4;
        }
    }
    
    // Verify new elements are zero-initialized
    for (int i = 3; i < 5; i++) {
        int value;
        int_array_get(arr, i, &value);
        if (value != 0) {
            error("New element at index %d not zero-initialized: %d != 0", i, value);
            ptk_local_free(&arr);
            return 5;
        }
    }
    
    // Test shrinking the array
    result = int_array_resize(arr, 2);
    if (result != PTK_OK) {
        error("int_array_resize (shrink) failed");
        ptk_local_free(&arr);
        return 6;
    }
    
    if (int_array_len(arr) != 2) {
        error("Array length after shrink incorrect: %zu != 2", int_array_len(arr));
        ptk_local_free(&arr);
        return 7;
    }
    
    // Verify remaining values are preserved
    for (int i = 0; i < 2; i++) {
        int value;
        int_array_get(arr, i, &value);
        if (value != i + 100) {
            error("Value at index %d changed after shrink: %d != %d", i, value, i + 100);
            ptk_local_free(&arr);
            return 8;
        }
    }
    
    // Test resize to zero (should fail)
    result = int_array_resize(arr, 0);
    if (result != PTK_ERR_INVALID_PARAM) {
        error("int_array_resize should fail for zero size");
        ptk_local_free(&arr);
        return 9;
    }
    
    // Test resize with NULL array
    result = int_array_resize(NULL, 10);
    if (result != PTK_ERR_NULL_PTR) {
        error("int_array_resize should return PTK_ERR_NULL_PTR for NULL array");
        ptk_local_free(&arr);
        return 10;
    }
    
    ptk_local_free(&arr);
    
    info("test_array_resize_operations exit");
    return 0;
}

int test_array_append_operations(void) {
    info("test_array_append_operations entry");
    
    int_array_t *arr = int_array_create(2, NULL);
    if (!arr) {
        error("int_array_create failed");
        return 1;
    }
    
    // Initialize with test values
    int_array_set(arr, 0, 10);
    int_array_set(arr, 1, 20);
    
    // Test appending elements
    ptk_err result = int_array_append(arr, 30);
    if (result != PTK_OK) {
        error("int_array_append failed");
        ptk_local_free(&arr);
        return 2;
    }
    
    if (int_array_len(arr) != 3) {
        error("Array length after append incorrect: %zu != 3", int_array_len(arr));
        ptk_local_free(&arr);
        return 3;
    }
    
    // Verify appended value
    int value;
    int_array_get(arr, 2, &value);
    if (value != 30) {
        error("Appended value incorrect: %d != 30", value);
        ptk_local_free(&arr);
        return 4;
    }
    
    // Test multiple appends
    for (int i = 0; i < 5; i++) {
        result = int_array_append(arr, 40 + i);
        if (result != PTK_OK) {
            error("int_array_append failed at iteration %d", i);
            ptk_local_free(&arr);
            return 5;
        }
    }
    
    if (int_array_len(arr) != 8) {
        error("Array length after multiple appends incorrect: %zu != 8", int_array_len(arr));
        ptk_local_free(&arr);
        return 6;
    }
    
    // Verify all values
    int expected[] = {10, 20, 30, 40, 41, 42, 43, 44};
    for (size_t i = 0; i < 8; i++) {
        int_array_get(arr, i, &value);
        if (value != expected[i]) {
            error("Value at index %zu incorrect: %d != %d", i, value, expected[i]);
            ptk_local_free(&arr);
            return 7;
        }
    }
    
    // Test append with NULL array
    result = int_array_append(NULL, 42);
    if (result != PTK_ERR_NULL_PTR) {
        error("int_array_append should return PTK_ERR_NULL_PTR for NULL array");
        ptk_local_free(&arr);
        return 8;
    }
    
    ptk_local_free(&arr);
    
    info("test_array_append_operations exit");
    return 0;
}

int test_array_copy_operations(void) {
    info("test_array_copy_operations entry");
    
    int_array_t *original = int_array_create(4, NULL);
    if (!original) {
        error("int_array_create failed");
        return 1;
    }
    
    // Initialize with test values
    for (int i = 0; i < 4; i++) {
        int_array_set(original, i, i * 5);
    }
    
    // Test int_array_copy
    int_array_t *copy = int_array_copy(original);
    if (!copy) {
        error("int_array_copy failed");
        ptk_local_free(&original);
        return 2;
    }
    
    // Verify copy has same length
    if (int_array_len(copy) != int_array_len(original)) {
        error("Copy length differs from original: %zu != %zu", 
              int_array_len(copy), int_array_len(original));
        ptk_local_free(&original);
        ptk_local_free(&copy);
        return 3;
    }
    
    // Verify copy has same values
    for (int i = 0; i < 4; i++) {
        int original_value, copy_value;
        int_array_get(original, i, &original_value);
        int_array_get(copy, i, &copy_value);
        
        if (original_value != copy_value) {
            error("Copy value differs from original at index %d: %d != %d", 
                  i, copy_value, original_value);
            ptk_local_free(&original);
            ptk_local_free(&copy);
            return 4;
        }
    }
    
    // Verify copy is independent (modify original)
    int_array_set(original, 0, 999);
    
    int original_value, copy_value;
    int_array_get(original, 0, &original_value);
    int_array_get(copy, 0, &copy_value);
    
    if (original_value == copy_value) {
        error("Copy is not independent from original");
        ptk_local_free(&original);
        ptk_local_free(&copy);
        return 5;
    }
    
    ptk_local_free(&original);
    ptk_local_free(&copy);
    
    // Test copy with NULL array
    int_array_t *null_copy = int_array_copy(NULL);
    if (null_copy != NULL) {
        error("int_array_copy should return NULL for NULL array");
        ptk_local_free(&null_copy);
        return 6;
    }
    
    // Test copy with zero-length array
    int_array_t *zero_arr = int_array_create(1, NULL);
    if (zero_arr) {
        // This simulates a zero-length array by setting internal state
        zero_arr->len = 0;
        int_array_t *zero_copy = int_array_copy(zero_arr);
        if (zero_copy != NULL) {
            error("int_array_copy should return NULL for zero-length array");
            ptk_local_free(&zero_copy);
        }
        ptk_local_free(&zero_arr);
    }
    
    info("test_array_copy_operations exit");
    return 0;
}

int test_array_from_raw_operations(void) {
    info("test_array_from_raw_operations entry");
    
    // Test int_array_from_raw
    int raw_data[] = {100, 200, 300, 400, 500};
    size_t raw_count = sizeof(raw_data) / sizeof(raw_data[0]);
    
    int_array_t *arr = int_array_from_raw(raw_data, raw_count, NULL);
    if (!arr) {
        error("int_array_from_raw failed");
        return 1;
    }
    
    // Verify length
    if (int_array_len(arr) != raw_count) {
        error("Array from raw has wrong length: %zu != %zu", int_array_len(arr), raw_count);
        ptk_local_free(&arr);
        return 2;
    }
    
    // Verify values
    for (size_t i = 0; i < raw_count; i++) {
        int value;
        int_array_get(arr, i, &value);
        if (value != raw_data[i]) {
            error("Array from raw has wrong value at index %zu: %d != %d", 
                  i, value, raw_data[i]);
            ptk_local_free(&arr);
            return 3;
        }
    }
    
    ptk_local_free(&arr);
    
    // Test with NULL raw data
    int_array_t *null_arr = int_array_from_raw(NULL, 5, NULL);
    if (null_arr != NULL) {
        error("int_array_from_raw should return NULL for NULL raw data");
        ptk_local_free(&null_arr);
        return 4;
    }
    
    // Test with zero count
    int_array_t *zero_arr = int_array_from_raw(raw_data, 0, NULL);
    if (zero_arr != NULL) {
        error("int_array_from_raw should return NULL for zero count");
        ptk_local_free(&zero_arr);
        return 5;
    }
    
    // Test with single element
    int single_data = 42;
    int_array_t *single_arr = int_array_from_raw(&single_data, 1, NULL);
    if (!single_arr) {
        error("int_array_from_raw failed for single element");
        return 6;
    }
    
    if (int_array_len(single_arr) != 1) {
        error("Single element array has wrong length: %zu != 1", int_array_len(single_arr));
        ptk_local_free(&single_arr);
        return 7;
    }
    
    int value;
    int_array_get(single_arr, 0, &value);
    if (value != 42) {
        error("Single element array has wrong value: %d != 42", value);
        ptk_local_free(&single_arr);
        return 8;
    }
    
    ptk_local_free(&single_arr);
    
    info("test_array_from_raw_operations exit");
    return 0;
}

//=============================================================================
// Complex Data Type Tests
//=============================================================================

int test_array_with_destructors(void) {
    info("test_array_with_destructors entry");
    
    // Test with complex data type that has destructor
    test_struct_array_t *arr = test_struct_array_create(2, test_struct_destructor);
    if (!arr) {
        error("test_struct_array_create failed");
        return 1;
    }
    
    // Create test structs with allocated memory
    test_struct_t struct1 = {42, ptk_local_alloc(10, NULL)};
    test_struct_t struct2 = {84, ptk_local_alloc(20, NULL)};
    
    if (!struct1.name || !struct2.name) {
        error("Failed to allocate memory for test structs");
        ptk_local_free(&arr);
        return 2;
    }
    
    strcpy(struct1.name, "first");
    strcpy(struct2.name, "second");
    
    // Set values in array
    test_struct_array_set(arr, 0, struct1);
    test_struct_array_set(arr, 1, struct2);
    
    // Verify values
    test_struct_t retrieved;
    test_struct_array_get(arr, 0, &retrieved);
    if (retrieved.value != 42 || strcmp(retrieved.name, "first") != 0) {
        error("Retrieved struct 1 has wrong values");
        ptk_local_free(&arr);
        return 3;
    }
    
    test_struct_array_get(arr, 1, &retrieved);
    if (retrieved.value != 84 || strcmp(retrieved.name, "second") != 0) {
        error("Retrieved struct 2 has wrong values");
        ptk_local_free(&arr);
        return 4;
    }
    
    // The destructor should be called automatically when array is freed
    ptk_local_free(&arr);
    
    info("test_array_with_destructors exit");
    return 0;
}

int test_array_with_different_types(void) {
    info("test_array_with_different_types entry");
    
    // Test with double array
    double_array_t *double_arr = double_array_create(3, NULL);
    if (!double_arr) {
        error("double_array_create failed");
        return 1;
    }
    
    // Test with floating point values
    double test_values[] = {3.14159, 2.71828, 1.41421};
    for (int i = 0; i < 3; i++) {
        double_array_set(double_arr, i, test_values[i]);
    }
    
    // Verify values
    for (int i = 0; i < 3; i++) {
        double value;
        double_array_get(double_arr, i, &value);
        if (value != test_values[i]) {
            error("Double array value at index %d incorrect: %f != %f", 
                  i, value, test_values[i]);
            ptk_local_free(&double_arr);
            return 2;
        }
    }
    
    // Test copy with different type
    double_array_t *double_copy = double_array_copy(double_arr);
    if (!double_copy) {
        error("double_array_copy failed");
        ptk_local_free(&double_arr);
        return 3;
    }
    
    // Verify copy
    for (int i = 0; i < 3; i++) {
        double original_value, copy_value;
        double_array_get(double_arr, i, &original_value);
        double_array_get(double_copy, i, &copy_value);
        
        if (original_value != copy_value) {
            error("Double array copy value differs at index %d: %f != %f", 
                  i, copy_value, original_value);
            ptk_local_free(&double_arr);
            ptk_local_free(&double_copy);
            return 4;
        }
    }
    
    ptk_local_free(&double_arr);
    ptk_local_free(&double_copy);
    
    info("test_array_with_different_types exit");
    return 0;
}

//=============================================================================
// Edge Cases and Error Handling
//=============================================================================

int test_array_edge_cases(void) {
    info("test_array_edge_cases entry");
    
    // Test with very large array
    int_array_t *large_arr = int_array_create(1000, NULL);
    if (!large_arr) {
        error("Failed to create large array");
        return 1;
    }
    
    // Test setting and getting from large array
    int_array_set(large_arr, 999, 12345);
    int value;
    int_array_get(large_arr, 999, &value);
    if (value != 12345) {
        error("Large array value incorrect: %d != 12345", value);
        ptk_local_free(&large_arr);
        return 2;
    }
    
    ptk_local_free(&large_arr);
    
    // Test validation with corrupted array
    int_array_t *arr = int_array_create(5, NULL);
    if (!arr) {
        error("int_array_create failed");
        return 3;
    }
    
    // Manually corrupt the array
    arr->len = 0;
    if (int_array_is_valid(arr)) {
        error("int_array_is_valid should return false for corrupted array");
        ptk_local_free(&arr);
        return 4;
    }
    
    // Restore and test
    arr->len = 5;
    if (!int_array_is_valid(arr)) {
        error("int_array_is_valid should return true for restored array");
        ptk_local_free(&arr);
        return 5;
    }
    
    ptk_local_free(&arr);
    
    info("test_array_edge_cases exit");
    return 0;
}

//=============================================================================
// Main Test Function
//=============================================================================

int test_ptk_array_main(void) {
    info("=== Starting PTK Array Tests ===");
    
    int result = 0;
    
    // Basic operations
    if ((result = test_array_creation_and_validation()) != 0) {
        error("test_array_creation_and_validation failed with code %d", result);
        return result;
    }
    
    if ((result = test_array_get_set_operations()) != 0) {
        error("test_array_get_set_operations failed with code %d", result);
        return result;
    }
    
    if ((result = test_array_resize_operations()) != 0) {
        error("test_array_resize_operations failed with code %d", result);
        return result;
    }
    
    if ((result = test_array_append_operations()) != 0) {
        error("test_array_append_operations failed with code %d", result);
        return result;
    }
    
    if ((result = test_array_copy_operations()) != 0) {
        error("test_array_copy_operations failed with code %d", result);
        return result;
    }
    
    if ((result = test_array_from_raw_operations()) != 0) {
        error("test_array_from_raw_operations failed with code %d", result);
        return result;
    }
    
    // Complex data types
    if ((result = test_array_with_destructors()) != 0) {
        error("test_array_with_destructors failed with code %d", result);
        return result;
    }
    
    if ((result = test_array_with_different_types()) != 0) {
        error("test_array_with_different_types failed with code %d", result);
        return result;
    }
    
    // Edge cases
    if ((result = test_array_edge_cases()) != 0) {
        error("test_array_edge_cases failed with code %d", result);
        return result;
    }
    
    info("=== All PTK Array Tests Passed ===");
    return 0;
}