/**
 * @file test_ptk_mem.c
 * @brief Tests for ptk_mem.h memory management APIs
 *
 * This file tests the local and shared memory allocation functions of the Protocol Toolkit.
 * Tests ptk_local_alloc, ptk_local_realloc, ptk_local_free, and shared memory APIs.
 */

#include <ptk_mem.h>
#include <ptk_log.h>
#include <ptk_err.h>
#include <ptk_os_thread.h>
#include <ptk_utils.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Global flag to test destructor calls
static int destructor_called = 0;

void test_destructor(void *ptr) {
    (void)ptr; // Unused parameter
    destructor_called = 1;
    info("Destructor called");
}

int test_local_alloc() {
    info("test_local_alloc entry");
    
    // Test basic allocation
    void *ptr = ptk_local_alloc(1024, NULL);
    if (!ptr) {
        error("ptk_local_alloc failed");
        return 1;
    }
    
    // Test writing to allocated memory
    memset(ptr, 0xAA, 1024);
    
    // Free the memory
    ptk_local_free(&ptr);
    if (ptr != NULL) {
        error("ptk_local_free did not set pointer to NULL");
        return 2;
    }
    
    info("test_local_alloc exit");
    return 0;
}

int test_local_alloc_with_destructor() {
    info("test_local_alloc_with_destructor entry");
    
    destructor_called = 0;
    void *ptr = ptk_local_alloc(512, test_destructor);
    if (!ptr) {
        error("ptk_local_alloc with destructor failed");
        return 1;
    }
    
    ptk_local_free(&ptr);
    if (!destructor_called) {
        error("Destructor was not called");
        return 2;
    }
    
    if (ptr != NULL) {
        error("ptk_local_free did not set pointer to NULL");
        return 3;
    }
    
    info("test_local_alloc_with_destructor exit");
    return 0;
}

int test_local_realloc() {
    info("test_local_realloc entry");
    
    // Start with small allocation
    void *ptr = ptk_local_alloc(100, NULL);
    if (!ptr) {
        error("ptk_local_alloc failed");
        return 1;
    }
    
    // Fill with test pattern
    memset(ptr, 0x55, 100);
    
    // Grow the allocation
    ptr = ptk_local_realloc(ptr, 200);
    if (!ptr) {
        error("ptk_local_realloc failed");
        return 2;
    }
    
    // Check that original data is preserved
    unsigned char *bytes = (unsigned char *)ptr;
    for (int i = 0; i < 100; i++) {
        if (bytes[i] != 0x55) {
            error("Data not preserved during realloc");
            ptk_local_free(&ptr);
            return 3;
        }
    }
    
    // Shrink the allocation
    ptr = ptk_local_realloc(ptr, 50);
    if (!ptr) {
        error("ptk_local_realloc shrink failed");
        return 4;
    }
    
    ptk_local_free(&ptr);
    info("test_local_realloc exit");
    return 0;
}

int test_shared_memory() {
    info("test_shared_memory entry");
    
    // Initialize shared memory subsystem
    ptk_err err = ptk_shared_init();
    if (err != PTK_OK) {
        error("ptk_shared_init failed");
        return 1;
    }
    
    // Allocate shared memory directly
    ptk_shared_handle_t handle = ptk_shared_alloc(sizeof(int) * 10, NULL);
    if (!PTK_SHARED_IS_VALID(handle)) {
        error("ptk_shared_alloc failed");
        ptk_shared_shutdown();
        return 2;
    }
    
    // Initialize test data using direct acquire/release
    int *data = ptk_shared_acquire(handle, PTK_TIME_WAIT_FOREVER);
    if (!data) {
        error("Failed to acquire shared memory for initialization");
        ptk_shared_release(handle);
        ptk_shared_shutdown();
        return 3;
    }
    
    for (int i = 0; i < 10; i++) {
        data[i] = i * 2;
    }
    ptk_shared_release(handle);
    
    // Test acquisition and usage
    int *shared_data = ptk_shared_acquire(handle, PTK_TIME_WAIT_FOREVER);
    if (!shared_data) {
        error("Failed to acquire shared memory for verification");
        ptk_shared_release(handle);
        ptk_shared_shutdown();
        return 4;
    }
    
    // Verify data integrity
    for (int i = 0; i < 10; i++) {
        if (shared_data[i] != i * 2) {
            error("Shared data corrupted");
            ptk_shared_release(handle);
            ptk_shared_release(handle);
            ptk_shared_shutdown();
            return 5;
        }
    }
    ptk_shared_release(handle);
    
    // Release the handle (should free the memory)
    ptk_shared_release(handle);
    
    // Shutdown shared memory subsystem
    ptk_shared_shutdown();
    
    info("test_shared_memory exit");
    return 0;
}

int test_use_shared_macro() {
    info("test_use_shared_macro entry");
    
    ptk_err err = ptk_shared_init();
    if (err != PTK_OK) {
        error("ptk_shared_init failed");
        return 1;
    }
    
    // Allocate shared memory for test
    ptk_shared_handle_t handle = ptk_shared_alloc(sizeof(int), NULL);
    if (!PTK_SHARED_IS_VALID(handle)) {
        error("ptk_shared_alloc failed");
        ptk_shared_shutdown();
        return 2;
    }
    
    // Initialize the data using direct acquire/release
    int *data = ptk_shared_acquire(handle, PTK_TIME_WAIT_FOREVER);
    if (!data) {
        error("Failed to acquire shared memory for initialization");
        ptk_shared_release(handle);
        ptk_shared_shutdown();
        return 3;
    }
    *data = 42;
    ptk_shared_release(handle);
    
    // Test shared memory access
    int *shared_ptr = ptk_shared_acquire(handle, PTK_TIME_WAIT_FOREVER);
    if (!shared_ptr) {
        error("Failed to acquire shared memory");
        ptk_shared_release(handle);
        ptk_shared_shutdown();
        return 4;
    }
    
    if (*shared_ptr != 42) {
        error("Shared memory value incorrect");
        ptk_shared_release(handle);
        ptk_shared_release(handle);
        ptk_shared_shutdown();
        return 5;
    }
    
    // Modify the data
    *shared_ptr = 99;
    ptk_shared_release(handle);
    
    // Verify the modification
    shared_ptr = ptk_shared_acquire(handle, PTK_TIME_WAIT_FOREVER);
    if (!shared_ptr) {
        error("Failed to reacquire shared memory");
        ptk_shared_release(handle);
        ptk_shared_shutdown();
        return 6;
    }
    
    if (*shared_ptr != 99) {
        error("Data modification failed");
        ptk_shared_release(handle);
        ptk_shared_release(handle);
        ptk_shared_shutdown();
        return 7;
    }
    ptk_shared_release(handle);
    
    ptk_shared_release(handle);
    ptk_shared_shutdown();
    
    info("test_use_shared_macro exit");
    return 0;
}

// Shared counter data structure
typedef struct {
    int counter;
} shared_counter_t;

// Thread data for multi-threaded test
typedef struct {
    ptk_shared_handle_t counter_handle;
    int iterations;
    int thread_id;
} thread_data_t;

void increment_thread(ptk_shared_handle_t param) {
    // Acquire the thread data from shared memory
    thread_data_t *data = ptk_shared_acquire(param, PTK_TIME_WAIT_FOREVER);
    if (!data) {
        error("Thread failed to acquire parameter data");
        return;
    }
    info("Thread %d starting with %d iterations", data->thread_id, data->iterations);
    
    for (int i = 0; i < data->iterations; i++) {
        shared_counter_t *shared_counter = ptk_shared_acquire(data->counter_handle, PTK_TIME_WAIT_FOREVER);
        if (!shared_counter) {
            error("Thread %d: Failed to acquire shared memory on iteration %d", data->thread_id, i);
            ptk_shared_release(param);
            return;
        }
        shared_counter->counter++;
        ptk_shared_release(data->counter_handle);
    }
    
    info("Thread %d completed", data->thread_id);
    ptk_shared_release(param);
}

int test_multithreaded_shared_memory() {
    info("test_multithreaded_shared_memory entry");
    
    // Initialize shared memory subsystem
    ptk_err err = ptk_shared_init();
    if (err != PTK_OK) {
        error("ptk_shared_init failed");
        return 1;
    }
    
    // Allocate shared counter
    ptk_shared_handle_t handle = ptk_shared_alloc(sizeof(shared_counter_t), NULL);
    if (!PTK_SHARED_IS_VALID(handle)) {
        error("ptk_shared_alloc failed");
        ptk_shared_shutdown();
        return 2;
    }
    
    // Initialize counter to 0
    shared_counter_t *counter = ptk_shared_acquire(handle, PTK_TIME_WAIT_FOREVER);
    if (!counter) {
        error("Failed to acquire shared counter for initialization");
        ptk_shared_release(handle);
        ptk_shared_shutdown();
        return 3;
    }
    counter->counter = 0;
    ptk_shared_release(handle);
    
    // Create shared thread data
    ptk_shared_handle_t thread1_data_handle = ptk_shared_alloc(sizeof(thread_data_t), NULL);
    ptk_shared_handle_t thread2_data_handle = ptk_shared_alloc(sizeof(thread_data_t), NULL);
    
    if (!PTK_SHARED_IS_VALID(thread1_data_handle) || !PTK_SHARED_IS_VALID(thread2_data_handle)) {
        error("Failed to allocate thread data");
        ptk_shared_release(handle);
        ptk_shared_shutdown();
        return 4;
    }
    
    // Initialize thread data
    thread_data_t *t1_data = ptk_shared_acquire(thread1_data_handle, PTK_TIME_WAIT_FOREVER);
    thread_data_t *t2_data = ptk_shared_acquire(thread2_data_handle, PTK_TIME_WAIT_FOREVER);
    
    t1_data->counter_handle = handle;
    t1_data->iterations = 100000;
    t1_data->thread_id = 1;
    
    t2_data->counter_handle = handle;
    t2_data->iterations = 100000;
    t2_data->thread_id = 2;
    
    ptk_shared_release(thread1_data_handle);
    ptk_shared_release(thread2_data_handle);
    
    // Create and start threads
    ptk_thread_handle_t thread1 = ptk_thread_create(increment_thread, thread1_data_handle);
    ptk_thread_handle_t thread2 = ptk_thread_create(increment_thread, thread2_data_handle);
    
    if (!PTK_SHARED_IS_VALID(thread1) || !PTK_SHARED_IS_VALID(thread2)) {
        error("Failed to create threads");
        ptk_shared_release(thread1_data_handle);
        ptk_shared_release(thread2_data_handle);
        ptk_shared_release(handle);
        ptk_shared_shutdown();
        return 5;
    }
    
    info("Waiting for threads to complete...");
    
    // Wait for threads to complete
    ptk_thread_wait(thread1, PTK_TIME_WAIT_FOREVER);
    ptk_thread_wait(thread2, PTK_TIME_WAIT_FOREVER);
    
    // Release thread handles and data
    ptk_shared_release(thread1);
    ptk_shared_release(thread2);
    ptk_shared_release(thread1_data_handle);
    ptk_shared_release(thread2_data_handle);
    
    // Check final result
    shared_counter_t *counter_ptr = ptk_shared_acquire(handle, PTK_TIME_WAIT_FOREVER);
    if (!counter_ptr) {
        error("Failed to read final counter value");
        ptk_shared_release(handle);
        ptk_shared_shutdown();
        return 5;
    }
    int final_count = counter_ptr->counter;
    ptk_shared_release(handle);
    
    info("Final counter value: %d (expected: 200000)", final_count);
    
    if (final_count != 200000) {
        error("Counter value is incorrect: %d != 200000", final_count);
        ptk_shared_release(handle);
        ptk_shared_shutdown();
        return 6;
    }
    
    // Clean up
    ptk_shared_release(handle);
    ptk_shared_shutdown();
    
    info("test_multithreaded_shared_memory exit");
    return 0;
}

int main() {
    info("=== Starting PTK Memory Management Tests ===");
    
    int result = 0;
    
    result = test_local_alloc();
    if (result != 0) {
        error("test_local_alloc failed with code %d", result);
        return result;
    }
    
    result = test_local_alloc_with_destructor();
    if (result != 0) {
        error("test_local_alloc_with_destructor failed with code %d", result);
        return result;
    }
    
    result = test_local_realloc();
    if (result != 0) {
        error("test_local_realloc failed with code %d", result);
        return result;
    }
    
    result = test_shared_memory();
    if (result != 0) {
        error("test_shared_memory failed with code %d", result);
        return result;
    }
    
    result = test_use_shared_macro();
    if (result != 0) {
        error("test_use_shared_macro failed with code %d", result);
        return result;
    }
    
    result = test_multithreaded_shared_memory();
    if (result != 0) {
        error("test_multithreaded_shared_memory failed with code %d", result);
        return result;
    }
    
    info("=== All PTK Memory Management Tests Passed ===");
    return 0;
}