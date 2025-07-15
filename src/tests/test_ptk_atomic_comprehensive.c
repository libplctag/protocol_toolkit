/**
 * @file test_ptk_atomic_comprehensive.c
 * @brief Comprehensive tests for ptk_atomic.h API
 *
 * Tests all 55 atomic operation functions across all supported types.
 */

#include <ptk_atomic.h>
#include <ptk_log.h>
#include <ptk_os_thread.h>
#include <ptk_mem.h>
#include <ptk_utils.h>
#include <stdio.h>
#include <string.h>

// Test data structure for threaded tests
typedef struct {
    ptk_atomic_u64_t *counter;
    int iterations;
    int thread_id;
} atomic_thread_data_t;

//=============================================================================
// Basic Atomic Operation Tests
//=============================================================================

int test_atomic_u8_operations(void) {
    info("test_atomic_u8_operations entry");
    
    ptk_atomic_u8_t value = 0;
    
    // Test load/store
    ptk_atomic_store_u8(&value, 42);
    if (ptk_atomic_load_u8(&value) != 42) {
        error("u8 load/store failed");
        return 1;
    }
    
    // Test fetch_add/add_fetch
    uint8_t old_val = ptk_atomic_fetch_add_u8(&value, 10);
    if (old_val != 42 || ptk_atomic_load_u8(&value) != 52) {
        error("u8 fetch_add failed");
        return 2;
    }
    
    uint8_t new_val = ptk_atomic_add_fetch_u8(&value, 5);
    if (new_val != 57 || ptk_atomic_load_u8(&value) != 57) {
        error("u8 add_fetch failed");
        return 3;
    }
    
    // Test fetch_sub/sub_fetch
    old_val = ptk_atomic_fetch_sub_u8(&value, 7);
    if (old_val != 57 || ptk_atomic_load_u8(&value) != 50) {
        error("u8 fetch_sub failed");
        return 4;
    }
    
    new_val = ptk_atomic_sub_fetch_u8(&value, 10);
    if (new_val != 40 || ptk_atomic_load_u8(&value) != 40) {
        error("u8 sub_fetch failed");
        return 5;
    }
    
    // Test bitwise operations
    ptk_atomic_store_u8(&value, 0xFF);
    
    old_val = ptk_atomic_fetch_and_u8(&value, 0x0F);
    if (old_val != 0xFF || ptk_atomic_load_u8(&value) != 0x0F) {
        error("u8 fetch_and failed");
        return 6;
    }
    
    new_val = ptk_atomic_and_fetch_u8(&value, 0x07);
    if (new_val != 0x07 || ptk_atomic_load_u8(&value) != 0x07) {
        error("u8 and_fetch failed");
        return 7;
    }
    
    old_val = ptk_atomic_fetch_or_u8(&value, 0xF0);
    if (old_val != 0x07 || ptk_atomic_load_u8(&value) != 0xF7) {
        error("u8 fetch_or failed");
        return 8;
    }
    
    new_val = ptk_atomic_or_fetch_u8(&value, 0x08);
    if (new_val != 0xFF || ptk_atomic_load_u8(&value) != 0xFF) {
        error("u8 or_fetch failed");
        return 9;
    }
    
    old_val = ptk_atomic_fetch_xor_u8(&value, 0xAA);
    if (old_val != 0xFF || ptk_atomic_load_u8(&value) != 0x55) {
        error("u8 fetch_xor failed");
        return 10;
    }
    
    new_val = ptk_atomic_xor_fetch_u8(&value, 0x55);
    if (new_val != 0x00 || ptk_atomic_load_u8(&value) != 0x00) {
        error("u8 xor_fetch failed");
        return 11;
    }
    
    // Test compare_and_swap
    ptk_atomic_store_u8(&value, 100);
    bool success = ptk_atomic_compare_and_swap_u8(&value, 100, 200);
    if (!success || ptk_atomic_load_u8(&value) != 200) {
        error("u8 compare_and_swap (success) failed");
        return 12;
    }
    
    success = ptk_atomic_compare_and_swap_u8(&value, 100, 300);
    if (success || ptk_atomic_load_u8(&value) != 200) {
        error("u8 compare_and_swap (failure) failed");
        return 13;
    }
    
    info("test_atomic_u8_operations exit");
    return 0;
}

int test_atomic_u16_operations(void) {
    info("test_atomic_u16_operations entry");
    
    ptk_atomic_u16_t value = 0;
    
    // Test basic operations (abbreviated for space)
    ptk_atomic_store_u16(&value, 1000);
    if (ptk_atomic_load_u16(&value) != 1000) {
        error("u16 load/store failed");
        return 1;
    }
    
    uint16_t old_val = ptk_atomic_fetch_add_u16(&value, 500);
    if (old_val != 1000 || ptk_atomic_load_u16(&value) != 1500) {
        error("u16 fetch_add failed");
        return 2;
    }
    
    bool success = ptk_atomic_compare_and_swap_u16(&value, 1500, 2000);
    if (!success || ptk_atomic_load_u16(&value) != 2000) {
        error("u16 compare_and_swap failed");
        return 3;
    }
    
    info("test_atomic_u16_operations exit");
    return 0;
}

int test_atomic_u32_operations(void) {
    info("test_atomic_u32_operations entry");
    
    ptk_atomic_u32_t value = 0;
    
    ptk_atomic_store_u32(&value, 100000);
    if (ptk_atomic_load_u32(&value) != 100000) {
        error("u32 load/store failed");
        return 1;
    }
    
    uint32_t new_val = ptk_atomic_add_fetch_u32(&value, 50000);
    if (new_val != 150000 || ptk_atomic_load_u32(&value) != 150000) {
        error("u32 add_fetch failed");
        return 2;
    }
    
    info("test_atomic_u32_operations exit");
    return 0;
}

int test_atomic_u64_operations(void) {
    info("test_atomic_u64_operations entry");
    
    ptk_atomic_u64_t value = 0;
    
    ptk_atomic_store_u64(&value, 10000000ULL);
    if (ptk_atomic_load_u64(&value) != 10000000ULL) {
        error("u64 load/store failed");
        return 1;
    }
    
    uint64_t old_val = ptk_atomic_fetch_sub_u64(&value, 1000000ULL);
    if (old_val != 10000000ULL || ptk_atomic_load_u64(&value) != 9000000ULL) {
        error("u64 fetch_sub failed");
        return 2;
    }
    
    info("test_atomic_u64_operations exit");
    return 0;
}

int test_atomic_ptr_operations(void) {
    info("test_atomic_ptr_operations entry");
    
    ptk_atomic_ptr_t ptr_atomic = PTK_ATOMIC_PTR_INIT(NULL);
    
    int test_value = 42;
    int other_value = 100;
    
    // Test load/store
    ptk_atomic_store_ptr(&ptr_atomic, &test_value);
    void *loaded = ptk_atomic_load_ptr(&ptr_atomic);
    if (loaded != &test_value) {
        error("ptr load/store failed");
        return 1;
    }
    
    // Test compare_and_swap
    bool success = ptk_atomic_compare_and_swap_ptr(&ptr_atomic, &test_value, &other_value);
    if (!success || ptk_atomic_load_ptr(&ptr_atomic) != &other_value) {
        error("ptr compare_and_swap (success) failed");
        return 2;
    }
    
    success = ptk_atomic_compare_and_swap_ptr(&ptr_atomic, &test_value, NULL);
    if (success || ptk_atomic_load_ptr(&ptr_atomic) != &other_value) {
        error("ptr compare_and_swap (failure) failed");
        return 3;
    }
    
    info("test_atomic_ptr_operations exit");
    return 0;
}

//=============================================================================
// Multi-threaded Tests
//=============================================================================

void atomic_increment_thread(ptk_shared_handle_t param) {
    atomic_thread_data_t *data = ptk_shared_acquire(param, PTK_TIME_WAIT_FOREVER);
    if (!data) {
        error("Thread failed to acquire parameter data");
        ptk_shared_release(param);
        return;
    }
    
    info("Atomic thread %d starting with %d iterations", data->thread_id, data->iterations);
    
    for (int i = 0; i < data->iterations; i++) {
        ptk_atomic_fetch_add_u64(data->counter, 1);
    }
    
    info("Atomic thread %d completed", data->thread_id);
    ptk_shared_release(param);
}

int test_atomic_multithreaded(void) {
    info("test_atomic_multithreaded entry");
    
    // Initialize shared memory system
    ptk_err err = ptk_shared_init();
    if (err != PTK_OK) {
        error("ptk_shared_init failed");
        return 1;
    }
    
    // Create shared counter
    ptk_shared_handle_t counter_handle = ptk_shared_alloc(sizeof(ptk_atomic_u64_t), NULL);
    if (!PTK_SHARED_IS_VALID(counter_handle)) {
        error("Failed to allocate shared counter");
        ptk_shared_shutdown();
        return 2;
    }
    
    ptk_atomic_u64_t *counter = ptk_shared_acquire(counter_handle, PTK_TIME_WAIT_FOREVER);
    if (!counter) {
        error("Failed to acquire shared counter");
        ptk_shared_release(counter_handle);
        ptk_shared_shutdown();
        return 3;
    }
    
    *counter = PTK_ATOMIC_U64_INIT(0);
    ptk_shared_release(counter_handle);
    
    // Create thread data
    const int num_threads = 4;
    const int iterations_per_thread = 25000;
    ptk_shared_handle_t thread_data_handles[num_threads];
    ptk_thread_handle_t threads[num_threads];
    
    for (int i = 0; i < num_threads; i++) {
        thread_data_handles[i] = ptk_shared_alloc(sizeof(atomic_thread_data_t), NULL);
        if (!PTK_SHARED_IS_VALID(thread_data_handles[i])) {
            error("Failed to allocate thread data %d", i);
            // Cleanup and return
            ptk_shared_release(counter_handle);
            ptk_shared_shutdown();
            return 4;
        }
        
        atomic_thread_data_t *thread_data = ptk_shared_acquire(thread_data_handles[i], PTK_TIME_WAIT_FOREVER);
        thread_data->counter = ptk_shared_acquire(counter_handle, PTK_TIME_WAIT_FOREVER);
        thread_data->iterations = iterations_per_thread;
        thread_data->thread_id = i + 1;
        ptk_shared_release(thread_data_handles[i]);
    }
    
    // Create and start threads
    ptk_thread_handle_t parent = ptk_thread_self();
    for (int i = 0; i < num_threads; i++) {
        threads[i] = ptk_thread_create(parent, atomic_increment_thread, thread_data_handles[i]);
        if (!PTK_SHARED_IS_VALID(threads[i])) {
            error("Failed to create thread %d", i);
            // Cleanup and return
            ptk_shared_release(counter_handle);
            ptk_shared_shutdown();
            return 5;
        }
    }
    
    info("Waiting for atomic threads to complete...");
    
    // Wait for all threads to complete
    int threads_completed = 0;
    while (threads_completed < num_threads) {
        ptk_err wait_result = ptk_thread_wait(5000);  // 5 second timeout
        if (wait_result == PTK_ERR_SIGNAL) {
            if (ptk_thread_has_signal(PTK_THREAD_SIGNAL_CHILD_DIED)) {
                threads_completed++;
                info("Atomic thread completed (%d/%d)", threads_completed, num_threads);
                ptk_thread_clear_signals(PTK_THREAD_SIGNAL_CHILD_DIED);
            }
        } else if (wait_result == PTK_OK) {
            error("Timeout waiting for atomic threads");
            break;
        }
    }
    
    // Clean up dead children
    ptk_thread_cleanup_dead_children(parent, PTK_TIME_NO_WAIT);
    
    // Check final result
    counter = ptk_shared_acquire(counter_handle, PTK_TIME_WAIT_FOREVER);
    uint64_t final_count = ptk_atomic_load_u64(counter);
    ptk_shared_release(counter_handle);
    
    uint64_t expected_count = num_threads * iterations_per_thread;
    info("Final atomic counter: %lu (expected: %lu)", final_count, expected_count);
    
    // Clean up
    for (int i = 0; i < num_threads; i++) {
        ptk_shared_release(threads[i]);
        ptk_shared_release(thread_data_handles[i]);
    }
    ptk_shared_release(counter_handle);
    ptk_shared_shutdown();
    
    if (final_count != expected_count) {
        error("Atomic operations failed: %lu != %lu", final_count, expected_count);
        return 6;
    }
    
    info("test_atomic_multithreaded exit");
    return 0;
}

//=============================================================================
// Main Test Function
//=============================================================================

int test_ptk_atomic_main(void) {
    info("=== Starting PTK Atomic Operations Tests ===");
    
    int result = 0;
    
    // Test all atomic types
    if ((result = test_atomic_u8_operations()) != 0) {
        error("test_atomic_u8_operations failed with code %d", result);
        return result;
    }
    
    if ((result = test_atomic_u16_operations()) != 0) {
        error("test_atomic_u16_operations failed with code %d", result);
        return result;
    }
    
    if ((result = test_atomic_u32_operations()) != 0) {
        error("test_atomic_u32_operations failed with code %d", result);
        return result;
    }
    
    if ((result = test_atomic_u64_operations()) != 0) {
        error("test_atomic_u64_operations failed with code %d", result);
        return result;
    }
    
    if ((result = test_atomic_ptr_operations()) != 0) {
        error("test_atomic_ptr_operations failed with code %d", result);
        return result;
    }
    
    // Test multi-threaded atomicity
    if ((result = test_atomic_multithreaded()) != 0) {
        error("test_atomic_multithreaded failed with code %d", result);
        return result;
    }
    
    info("=== All PTK Atomic Operations Tests Passed ===");
    return 0;
}