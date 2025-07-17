/**
 * @file test_ptk_err_comprehensive.c
 * @brief Comprehensive tests for ptk_err.h API
 *
 * Tests all error handling functionality including thread-local error storage.
 */

#include <ptk_err.h>
#include <ptk_log.h>
#include <ptk_os_thread.h>
#include <ptk_mem.h>
#include <ptk_utils.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

//=============================================================================
// Basic Error Handling Tests
//=============================================================================

int test_error_basic_operations(void) {
    info("test_error_basic_operations entry");
    
    // Initial state should be PTK_OK
    if (ptk_get_err() != PTK_OK) {
        error("Initial error state should be PTK_OK, got %d", ptk_get_err());
        return 1;
    }
    
    // Test setting and getting various error codes
    ptk_err_t test_errors[] = {
        PTK_ERR_INVALID_PARAM,
        PTK_ERR_NO_RESOURCES,
        PTK_ERR_TIMEOUT,
        PTK_ERR_NETWORK_ERROR,
        PTK_ERR_ABORT
    };
    
    for (size_t i = 0; i < sizeof(test_errors)/sizeof(test_errors[0]); i++) {
        ptk_set_err(test_errors[i]);
        ptk_err_t retrieved = ptk_get_err();
        
        if (retrieved != test_errors[i]) {
            error("Error code mismatch: set %d, got %d", test_errors[i], retrieved);
            return 2;
        }
    }
    
    // Reset to OK
    ptk_set_err(PTK_OK);
    if (ptk_get_err() != PTK_OK) {
        error("Failed to reset error to PTK_OK");
        return 3;
    }
    
    info("test_error_basic_operations exit");
    return 0;
}

int test_error_string_conversion(void) {
    info("test_error_string_conversion entry");
    
    // Test string conversion for known error codes
    const char *ok_str = ptk_err_to_string(PTK_OK);
    if (!ok_str || strlen(ok_str) == 0) {
        error("PTK_OK string conversion failed");
        return 1;
    }
    
    const char *invalid_str = ptk_err_to_string(PTK_ERR_INVALID_PARAM);
    if (!invalid_str || strlen(invalid_str) == 0) {
        error("PTK_ERR_INVALID_PARAM string conversion failed");
        return 2;
    }
    
    const char *timeout_str = ptk_err_to_string(PTK_ERR_TIMEOUT);
    if (!timeout_str || strlen(timeout_str) == 0) {
        error("PTK_ERR_TIMEOUT string conversion failed");
        return 3;
    }
    
    // Verify that different errors have different strings
    if (strcmp(ok_str, invalid_str) == 0) {
        error("Different error codes should have different strings");
        return 4;
    }
    
    if (strcmp(invalid_str, timeout_str) == 0) {
        error("Different error codes should have different strings");
        return 5;
    }
    
    info("PTK_OK string: '%s'", ok_str);
    info("PTK_ERR_INVALID_PARAM string: '%s'", invalid_str);
    info("PTK_ERR_TIMEOUT string: '%s'", timeout_str);
    
    info("test_error_string_conversion exit");
    return 0;
}

int test_all_error_codes(void) {
    info("test_all_error_codes entry");
    
    // Test that all defined error codes have valid string representations
    ptk_err_t all_errors[] = {
        PTK_OK,
        PTK_ERR_ABORT,
        PTK_ERR_ADDRESS_IN_USE,
        PTK_ERR_AUTHENTICATION_FAILED,
        PTK_ERR_AUTHORIZATION_FAILED,
        PTK_ERR_BAD_FORMAT,
        PTK_ERR_BAD_INTERNAL_STATE,
        PTK_ERR_BUSY,
        PTK_ERR_CANCELED,
        PTK_ERR_BUFFER_TOO_SMALL,
        PTK_ERR_CHECKSUM_FAILED,
        PTK_ERR_CLOSED,
        PTK_ERR_CONFIGURATION_ERROR,
        PTK_ERR_CONNECTION_REFUSED,
        PTK_ERR_DEVICE_BUSY,
        PTK_ERR_DEVICE_FAILURE,
        PTK_ERR_HOST_UNREACHABLE,
        PTK_ERR_INTERRUPT,
        PTK_ERR_INVALID_PARAM,
        PTK_ERR_NETWORK_ERROR,
        PTK_ERR_NO_RESOURCES,
        PTK_ERR_NULL_PTR,
        PTK_ERR_OUT_OF_BOUNDS,
        PTK_ERR_PARSE_ERROR,
        PTK_ERR_PROTOCOL_ERROR,
        PTK_ERR_RATE_LIMITED,
        PTK_ERR_SEQUENCE_ERROR,
        PTK_ERR_SIGNAL,
        PTK_ERR_TIMEOUT,
        PTK_ERR_UNSUPPORTED,
        PTK_ERR_UNSUPPORTED_VERSION,
        PTK_ERR_VALIDATION,
        PTK_ERR_WOULD_BLOCK
    };
    
    for (size_t i = 0; i < sizeof(all_errors)/sizeof(all_errors[0]); i++) {
        const char *err_str = ptk_err_to_string(all_errors[i]);
        if (!err_str || strlen(err_str) == 0) {
            error("Error code %d has no string representation", all_errors[i]);
            return 1;
        }
        
        // Verify we can set and get each error code
        ptk_set_err(all_errors[i]);
        if (ptk_get_err() != all_errors[i]) {
            error("Failed to set/get error code %d", all_errors[i]);
            return 2;
        }
    }
    
    info("All %zu error codes have valid string representations", 
         sizeof(all_errors)/sizeof(all_errors[0]));
    
    info("test_all_error_codes exit");
    return 0;
}

//=============================================================================
// Thread-Local Error Storage Tests
//=============================================================================

typedef struct {
    ptk_err_t error_to_set;
    ptk_err_t expected_error;
    int thread_id;
    bool test_passed;
} error_thread_data_t;

void error_thread_func(void) {
    // Use ptk_thread_get_handle_arg(0) to get the argument
    ptk_shared_handle_t param = ptk_thread_get_handle_arg(0);
    if (!ptk_shared_is_valid(param)) {
        error("Thread failed to get parameter handle");
        return;
    }
    
    error_thread_data_t *data = ptk_shared_acquire(param, PTK_TIME_WAIT_FOREVER);
    if (!data) {
        ptk_shared_release(param);
        return;
    }
    
    info("Error thread %d starting", data->thread_id);
    
    // Initial error should be PTK_OK in new thread
    if (ptk_get_err() != PTK_OK) {
        error("Thread %d: Initial error should be PTK_OK, got %d", 
              data->thread_id, ptk_get_err());
        data->test_passed = false;
        ptk_shared_release(param);
        return;
    }
    
    // Set the specified error
    ptk_set_err(data->error_to_set);
    
    // Verify it was set correctly
    ptk_err_t retrieved = ptk_get_err();
    if (retrieved != data->expected_error) {
        error("Thread %d: Error mismatch: set %d, expected %d, got %d", 
              data->thread_id, data->error_to_set, data->expected_error, retrieved);
        data->test_passed = false;
        ptk_shared_release(param);
        return;
    }
    
    // Sleep briefly to ensure other threads can run
    ptk_sleep_ms(100);  // 100ms

    // Error should still be the same (thread-local)
    retrieved = ptk_get_err();
    if (retrieved != data->expected_error) {
        error("Thread %d: Error changed unexpectedly: expected %d, got %d", 
              data->thread_id, data->expected_error, retrieved);
        data->test_passed = false;
        ptk_shared_release(param);
        return;
    }
    
    data->test_passed = true;
    info("Error thread %d completed successfully", data->thread_id);
    ptk_shared_release(param);
}

int test_thread_local_errors(void) {
    info("test_thread_local_errors entry");
    
    // Initialize shared memory system
    ptk_err_t err = ptk_shared_init();
    if (err != PTK_OK) {
        error("ptk_shared_init failed");
        return 1;
    }
    
    // Create thread data for multiple threads with different error codes
    const int num_threads = 3;
    ptk_err_t error_codes[] = {PTK_ERR_INVALID_PARAM, PTK_ERR_TIMEOUT, PTK_ERR_NO_RESOURCES};
    
    ptk_shared_handle_t thread_data_handles[num_threads];
    ptk_thread_handle_t threads[num_threads];
    
    // Set up thread data
    for (int i = 0; i < num_threads; i++) {
        thread_data_handles[i] = ptk_shared_alloc(sizeof(error_thread_data_t), NULL);
        if (!ptk_shared_is_valid(thread_data_handles[i])) {
            error("Failed to allocate thread data %d", i);
            ptk_shared_shutdown();
            return 2;
        }
        
        error_thread_data_t *thread_data = ptk_shared_acquire(thread_data_handles[i], PTK_TIME_WAIT_FOREVER);
        thread_data->error_to_set = error_codes[i];
        thread_data->expected_error = error_codes[i];
        thread_data->thread_id = i + 1;
        thread_data->test_passed = false;
        ptk_shared_release(thread_data_handles[i]);
    }
    
    // Set an error in the main thread
    ptk_set_err(PTK_ERR_BUSY);
    
    // Create and start threads
    ptk_thread_handle_t parent = ptk_thread_self();
    for (int i = 0; i < num_threads; i++) {
        threads[i] = ptk_thread_create();
        if (!ptk_shared_is_valid(threads[i])) {
            error("Failed to create thread %d", i);
            ptk_shared_shutdown();
            return 3;
        }
        
        ptk_err_t err = ptk_thread_add_handle_arg(threads[i], 0, &thread_data_handles[i]);
        if (err != PTK_OK) {
            error("Failed to add handle arg to thread %d: %d", i, err);
            ptk_shared_release(threads[i]);
            ptk_shared_shutdown();
            return 3;
        }
        
        err = ptk_thread_set_run_function(threads[i], error_thread_func);
        if (err != PTK_OK) {
            error("Failed to set run function for thread %d: %d", i, err);
            ptk_shared_release(threads[i]);
            ptk_shared_shutdown();
            return 3;
        }
        
        err = ptk_thread_start(threads[i]);
        if (err != PTK_OK) {
            error("Failed to start thread %d: %d", i, err);
            ptk_shared_release(threads[i]);
            ptk_shared_shutdown();
            return 3;
        }
    }
    
    info("Waiting for error threads to complete...");
    
    // Wait for all threads to complete
    int threads_completed = 0;
    while (threads_completed < num_threads) {
        ptk_err_t wait_result = ptk_thread_wait(5000);  // 5 second timeout
        if (wait_result == PTK_ERR_SIGNAL) {
            if (ptk_thread_has_signal(PTK_THREAD_SIGNAL_CHILD_DIED)) {
                threads_completed++;
                info("Error thread completed (%d/%d)", threads_completed, num_threads);
                ptk_thread_clear_signals(PTK_THREAD_SIGNAL_CHILD_DIED);
            }
        } else if (wait_result == PTK_OK) {
            error("Timeout waiting for error threads");
            break;
        }
    }
    
    // Verify main thread error is unchanged
    if (ptk_get_err() != PTK_ERR_BUSY) {
        error("Main thread error changed: expected PTK_ERR_BUSY, got %d", ptk_get_err());
        ptk_shared_shutdown();
        return 4;
    }
    
    // Check thread results
    bool all_passed = true;
    for (int i = 0; i < num_threads; i++) {
        error_thread_data_t *thread_data = ptk_shared_acquire(thread_data_handles[i], PTK_TIME_WAIT_FOREVER);
        if (!thread_data->test_passed) {
            error("Thread %d failed its error handling test", i + 1);
            all_passed = false;
        }
        ptk_shared_release(thread_data_handles[i]);
    }
    
    // Clean up
    ptk_thread_cleanup_dead_children(parent, PTK_TIME_NO_WAIT);
    for (int i = 0; i < num_threads; i++) {
        ptk_shared_release(threads[i]);
        ptk_shared_release(thread_data_handles[i]);
    }
    ptk_shared_shutdown();
    
    if (!all_passed) {
        return 5;
    }
    
    info("test_thread_local_errors exit");
    return 0;
}

//=============================================================================
// Main Test Function
//=============================================================================

int test_ptk_err_main(void) {
    info("=== Starting PTK Error Handling Tests ===");
    
    int result = 0;
    
    // Basic error operations
    if ((result = test_error_basic_operations()) != 0) {
        error("test_error_basic_operations failed with code %d", result);
        return result;
    }
    
    if ((result = test_error_string_conversion()) != 0) {
        error("test_error_string_conversion failed with code %d", result);
        return result;
    }
    
    if ((result = test_all_error_codes()) != 0) {
        error("test_all_error_codes failed with code %d", result);
        return result;
    }
    
    // Thread-local error storage
    if ((result = test_thread_local_errors()) != 0) {
        error("test_thread_local_errors failed with code %d", result);
        return result;
    }
    
    info("=== All PTK Error Handling Tests Passed ===");
    return 0;
}

int main(void) {
    return test_ptk_err_main();
}