/**
 * @file test_ptk_os_thread_comprehensive.c
 * @brief Comprehensive tests for ptk_os_thread.h API
 *
 * Tests all threading operations including thread creation, signaling,
 * parent-child relationships, and advanced threading scenarios.
 */

#include <ptk_os_thread.h>
#include <ptk_log.h>
#include <ptk_err.h>
#include <ptk_mem.h>
#include <ptk_utils.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// Test data structures
typedef struct {
    int thread_id;
    int iterations;
    bool should_signal_parent;
    ptk_thread_signal_t signal_to_send;
    ptk_shared_handle_t result_handle;
} thread_test_data_t;

typedef struct {
    int value;
    bool completed;
} thread_result_t;

//=============================================================================
// Thread Functions for Testing
//=============================================================================

void basic_thread_func(void) {
    // Use ptk_thread_get_handle_arg(0) to get the argument
    ptk_shared_handle_t param = ptk_thread_get_handle_arg(0);
    if (!ptk_shared_is_valid(param)) {
        error("Basic thread failed to get parameter handle");
        return;
    }
    
    thread_test_data_t *data = ptk_shared_acquire(param, PTK_TIME_WAIT_FOREVER);
    if (!data) {
        error("Basic thread failed to acquire parameter data");
        ptk_shared_release(param);
        return;
    }
    
    info("Basic thread %d starting", data->thread_id);
    
    // Simulate some work
    usleep(10000); // 10ms
    
    // Set result
    if (ptk_shared_is_valid(data->result_handle)) {
        thread_result_t *result = ptk_shared_acquire(data->result_handle, PTK_TIME_WAIT_FOREVER);
        if (result) {
            result->value = data->thread_id * 100;
            result->completed = true;
            ptk_shared_release(data->result_handle);
        }
    }
    
    info("Basic thread %d completed", data->thread_id);
    ptk_shared_release(param);
}

void signaling_thread_func(void) {
    // Use ptk_thread_get_handle_arg(0) to get the argument
    ptk_shared_handle_t param = ptk_thread_get_handle_arg(0);
    if (!ptk_shared_is_valid(param)) {
        error("Signaling thread failed to get parameter handle");
        return;
    }
    
    thread_test_data_t *data = ptk_shared_acquire(param, PTK_TIME_WAIT_FOREVER);
    if (!data) {
        error("Signaling thread failed to acquire parameter data");
        ptk_shared_release(param);
        return;
    }
    
    info("Signaling thread %d starting", data->thread_id);
    
    // Do some work
    for (int i = 0; i < data->iterations; i++) {
        usleep(1000); // 1ms per iteration
    }
    
    // Signal parent if requested
    if (data->should_signal_parent) {
        ptk_thread_handle_t parent = ptk_thread_get_parent(ptk_thread_self());
        if (ptk_shared_is_valid(parent)) {
            info("Thread %d signaling parent with signal %d", data->thread_id, data->signal_to_send);
            ptk_thread_signal(parent, data->signal_to_send);
        }
    }
    
    info("Signaling thread %d completed", data->thread_id);
    ptk_shared_release(param);
}

void long_running_thread_func(void) {
    // Use ptk_thread_get_handle_arg(0) to get the argument
    ptk_shared_handle_t param = ptk_thread_get_handle_arg(0);
    if (!ptk_shared_is_valid(param)) {
        error("Long running thread failed to get parameter handle");
        return;
    }
    
    thread_test_data_t *data = ptk_shared_acquire(param, PTK_TIME_WAIT_FOREVER);
    if (!data) {
        error("Long running thread failed to acquire parameter data");
        ptk_shared_release(param);
        return;
    }
    
    info("Long running thread %d starting", data->thread_id);
    
    // Run until signaled to stop
    while (true) {
        // Check for abort signals
        if (ptk_thread_has_signal(PTK_THREAD_SIGNAL_ABORT)) {
            info("Long running thread %d received abort signal", data->thread_id);
            break;
        }
        
        if (ptk_thread_has_signal(PTK_THREAD_SIGNAL_TERMINATE)) {
            info("Long running thread %d received terminate signal", data->thread_id);
            break;
        }
        
        // Do some work
        usleep(5000); // 5ms
    }
    
    info("Long running thread %d completed", data->thread_id);
    ptk_shared_release(param);
}

//=============================================================================
// Basic Threading Tests
//=============================================================================

int test_thread_creation_and_self(void) {
    info("test_thread_creation_and_self entry");
    
    // Initialize shared memory
    ptk_err_t err = ptk_shared_init();
    if (err != PTK_OK) {
        error("ptk_shared_init failed");
        return 1;
    }
    
    // Test ptk_thread_self
    ptk_thread_handle_t self = ptk_thread_self();
    (void)self;
    if (!ptk_shared_is_valid(self)) {
        error("ptk_thread_self returned invalid handle");
        ptk_shared_shutdown();
        return 2;
    }
    
    info("Main thread handle obtained successfully");
    
    // Create thread data
    ptk_shared_handle_t thread_data = ptk_shared_alloc(sizeof(thread_test_data_t), NULL);
    if (!ptk_shared_is_valid(thread_data)) {
        error("Failed to allocate thread data");
        ptk_shared_shutdown();
        return 3;
    }
    
    ptk_shared_handle_t result_handle = ptk_shared_alloc(sizeof(thread_result_t), NULL);
    if (!ptk_shared_is_valid(result_handle)) {
        error("Failed to allocate result handle");
        ptk_shared_release(thread_data);
        ptk_shared_shutdown();
        return 4;
    }
    
    // Initialize thread data
    thread_test_data_t *data = ptk_shared_acquire(thread_data, PTK_TIME_WAIT_FOREVER);
    data->thread_id = 1;
    data->iterations = 10;
    data->should_signal_parent = false;
    data->result_handle = result_handle;
    ptk_shared_release(thread_data);
    
    // Initialize result
    thread_result_t *result = ptk_shared_acquire(result_handle, PTK_TIME_WAIT_FOREVER);
    result->value = 0;
    result->completed = false;
    ptk_shared_release(result_handle);
    
    // Test ptk_thread_create
    ptk_thread_handle_t child = ptk_thread_create();
    if (!ptk_shared_is_valid(child)) {
        error("ptk_thread_create failed");
        ptk_shared_release(thread_data);
        ptk_shared_release(result_handle);
        ptk_shared_shutdown();
        return 5;
    }
    
    err = ptk_thread_add_handle_arg(child, 0, &thread_data);
    if (err != PTK_OK) {
        error("Failed to add handle arg to child thread: %d", err);
        ptk_shared_release(child);
        ptk_shared_release(thread_data);
        ptk_shared_release(result_handle);
        ptk_shared_shutdown();
        return 5;
    }
    
    err = ptk_thread_set_run_function(child, basic_thread_func);
    if (err != PTK_OK) {
        error("Failed to set run function for child thread: %d", err);
        ptk_shared_release(child);
        ptk_shared_release(thread_data);
        ptk_shared_release(result_handle);
        ptk_shared_shutdown();
        return 5;
    }
    
    err = ptk_thread_start(child);
    if (err != PTK_OK) {
        error("Failed to start child thread: %d", err);
        ptk_shared_release(child);
        ptk_shared_release(thread_data);
        ptk_shared_release(result_handle);
        ptk_shared_shutdown();
        return 5;
    }
    
    info("Child thread created successfully");
    
    // Wait for thread to complete
    ptk_err_t wait_result = ptk_thread_wait(5000); // 5 second timeout
    if (wait_result != PTK_ERR_SIGNAL) {
        error("ptk_thread_wait failed or timed out");
        ptk_shared_release(thread_data);
        ptk_shared_release(result_handle);
        ptk_shared_release(child);
        ptk_shared_shutdown();
        return 6;
    }
    
    // Check if we got child died signal
    if (ptk_thread_has_signal(PTK_THREAD_SIGNAL_CHILD_DIED)) {
        info("Received child died signal");
        ptk_thread_clear_signals(PTK_THREAD_SIGNAL_CHILD_DIED);
    }
    
    // Verify result
    result = ptk_shared_acquire(result_handle, PTK_TIME_WAIT_FOREVER);
    if (!result->completed || result->value != 100) {
        error("Thread result incorrect: completed=%s, value=%d", 
              result->completed ? "true" : "false", result->value);
        ptk_shared_release(result_handle);
        ptk_shared_release(thread_data);
        ptk_shared_release(child);
        ptk_shared_shutdown();
        return 7;
    }
    ptk_shared_release(result_handle);
    
    // Clean up
    ptk_thread_cleanup_dead_children(self, PTK_TIME_NO_WAIT);
    ptk_shared_release(thread_data);
    ptk_shared_release(result_handle);
    ptk_shared_release(child);
    ptk_shared_shutdown();
    
    info("test_thread_creation_and_self exit");
    return 0;
}

int test_thread_signaling(void) {
    info("test_thread_signaling entry");
    
    // Initialize shared memory
    ptk_err_t err = ptk_shared_init();
    if (err != PTK_OK) {
        error("ptk_shared_init failed");
        return 1;
    }
    
    ptk_thread_handle_t self = ptk_thread_self();
    
    // Create thread data
    ptk_shared_handle_t thread_data = ptk_shared_alloc(sizeof(thread_test_data_t), NULL);
    if (!ptk_shared_is_valid(thread_data)) {
        error("Failed to allocate thread data");
        ptk_shared_shutdown();
        return 2;
    }
    
    // Initialize thread data
    thread_test_data_t *data = ptk_shared_acquire(thread_data, PTK_TIME_WAIT_FOREVER);
    data->thread_id = 2;
    data->iterations = 5;
    data->should_signal_parent = true;
    data->signal_to_send = PTK_THREAD_SIGNAL_WAKE;
    data->result_handle = PTK_SHARED_INVALID_HANDLE;
    ptk_shared_release(thread_data);
    
    // Create thread
    ptk_thread_handle_t child = ptk_thread_create();
    if (!ptk_shared_is_valid(child)) {
        error("ptk_thread_create failed");
        ptk_shared_release(thread_data);
        ptk_shared_shutdown();
        return 3;
    }
    
    err = ptk_thread_add_handle_arg(child, 0, &thread_data);
    if (err != PTK_OK) {
        error("Failed to add handle arg to child thread: %d", err);
        ptk_shared_release(child);
        ptk_shared_release(thread_data);
        ptk_shared_shutdown();
        return 3;
    }
    
    err = ptk_thread_set_run_function(child, signaling_thread_func);
    if (err != PTK_OK) {
        error("Failed to set run function for child thread: %d", err);
        ptk_shared_release(child);
        ptk_shared_release(thread_data);
        ptk_shared_shutdown();
        return 3;
    }
    
    err = ptk_thread_start(child);
    if (err != PTK_OK) {
        error("Failed to start child thread: %d", err);
        ptk_shared_release(child);
        ptk_shared_release(thread_data);
        ptk_shared_shutdown();
        return 3;
    }
    
    info("Waiting for thread to signal us...");
    
    // Wait for signal
    ptk_err_t wait_result = ptk_thread_wait(5000); // 5 second timeout
    if (wait_result != PTK_ERR_SIGNAL) {
        error("ptk_thread_wait failed or timed out");
        ptk_shared_release(thread_data);
        ptk_shared_release(child);
        ptk_shared_shutdown();
        return 4;
    }
    
    // Test ptk_thread_has_signal
    if (ptk_thread_has_signal(PTK_THREAD_SIGNAL_WAKE)) {
        info("Received WAKE signal from child thread");
        
        // Test ptk_thread_clear_signals
        ptk_thread_clear_signals(PTK_THREAD_SIGNAL_WAKE);
        
        if (ptk_thread_has_signal(PTK_THREAD_SIGNAL_WAKE)) {
            error("Signal not cleared properly");
            ptk_shared_release(thread_data);
            ptk_shared_release(child);
            ptk_shared_shutdown();
            return 5;
        }
    } else {
        error("Expected WAKE signal not received");
        ptk_shared_release(thread_data);
        ptk_shared_release(child);
        ptk_shared_shutdown();
        return 6;
    }
    
    // Wait for thread to complete
    wait_result = ptk_thread_wait(5000);
    if (wait_result == PTK_ERR_SIGNAL && ptk_thread_has_signal(PTK_THREAD_SIGNAL_CHILD_DIED)) {
        info("Child thread completed");
        ptk_thread_clear_signals(PTK_THREAD_SIGNAL_CHILD_DIED);
    }
    
    // Test ptk_thread_get_pending_signals
    uint64_t pending = ptk_thread_get_pending_signals();
    info("Pending signals: 0x%lx", pending);
    
    // Clean up
    ptk_thread_cleanup_dead_children(self, PTK_TIME_NO_WAIT);
    ptk_shared_release(thread_data);
    ptk_shared_release(child);
    ptk_shared_shutdown();
    
    info("test_thread_signaling exit");
    return 0;
}

int test_thread_parent_child_relationships(void) {
    info("test_thread_parent_child_relationships entry");
    
    // Initialize shared memory
    ptk_err_t err = ptk_shared_init();
    if (err != PTK_OK) {
        error("ptk_shared_init failed");
        return 1;
    }
    
    ptk_thread_handle_t self = ptk_thread_self();
    
    // Test ptk_thread_count_children initially
    int initial_count = ptk_thread_count_children(self);
    info("Initial child count: %d", initial_count);
    
    // Create multiple child threads
    const int num_children = 3;
    ptk_thread_handle_t children[num_children];
    ptk_shared_handle_t thread_data_handles[num_children];
    
    for (int i = 0; i < num_children; i++) {
        thread_data_handles[i] = ptk_shared_alloc(sizeof(thread_test_data_t), NULL);
        if (!ptk_shared_is_valid(thread_data_handles[i])) {
            error("Failed to allocate thread data %d", i);
            // Clean up previous allocations
            for (int j = 0; j < i; j++) {
                ptk_shared_release(thread_data_handles[j]);
            }
            ptk_shared_shutdown();
            return 2;
        }
        
        thread_test_data_t *data = ptk_shared_acquire(thread_data_handles[i], PTK_TIME_WAIT_FOREVER);
        data->thread_id = i + 1;
        data->iterations = 10;
        data->should_signal_parent = false;
        data->result_handle = PTK_SHARED_INVALID_HANDLE;
        ptk_shared_release(thread_data_handles[i]);
        
        children[i] = ptk_thread_create();
        if (!ptk_shared_is_valid(children[i])) {
            error("Failed to create child thread %d", i);
            // Clean up
            for (int j = 0; j <= i; j++) {
                ptk_shared_release(thread_data_handles[j]);
            }
            for (int j = 0; j < i; j++) {
                ptk_shared_release(children[j]);
            }
            ptk_shared_shutdown();
            return 3;
        }
        
        err = ptk_thread_add_handle_arg(children[i], 0, &thread_data_handles[i]);
        if (err != PTK_OK) {
            error("Failed to add handle arg to child thread %d: %d", i, err);
            ptk_shared_release(children[i]);
            // Clean up
            for (int j = 0; j <= i; j++) {
                ptk_shared_release(thread_data_handles[j]);
            }
            for (int j = 0; j < i; j++) {
                ptk_shared_release(children[j]);
            }
            ptk_shared_shutdown();
            return 3;
        }
        
        err = ptk_thread_set_run_function(children[i], basic_thread_func);
        if (err != PTK_OK) {
            error("Failed to set run function for child thread %d: %d", i, err);
            ptk_shared_release(children[i]);
            // Clean up
            for (int j = 0; j <= i; j++) {
                ptk_shared_release(thread_data_handles[j]);
            }
            for (int j = 0; j < i; j++) {
                ptk_shared_release(children[j]);
            }
            ptk_shared_shutdown();
            return 3;
        }
        
        err = ptk_thread_start(children[i]);
        if (err != PTK_OK) {
            error("Failed to start child thread %d: %d", i, err);
            ptk_shared_release(children[i]);
            // Clean up
            for (int j = 0; j <= i; j++) {
                ptk_shared_release(thread_data_handles[j]);
            }
            for (int j = 0; j < i; j++) {
                ptk_shared_release(children[j]);
            }
            ptk_shared_shutdown();
            return 3;
        }
    }
    
    // Test ptk_thread_count_children after creating threads
    int child_count = ptk_thread_count_children(self);
    info("Child count after creation: %d", child_count);
    
    if (child_count != initial_count + num_children) {
        error("Child count incorrect: %d != %d", child_count, initial_count + num_children);
        // Clean up
        for (int i = 0; i < num_children; i++) {
            ptk_shared_release(thread_data_handles[i]);
            ptk_shared_release(children[i]);
        }
        ptk_shared_shutdown();
        return 4;
    }
    
    // Test ptk_thread_get_parent from child thread perspective
    // (This is tricky to test directly, but we can verify the parent-child relationship)
    
    // Wait for all children to complete
    int completed_children = 0;
    while (completed_children < num_children) {
        ptk_err_t wait_result = ptk_thread_wait(5000);
        if (wait_result == PTK_ERR_SIGNAL && ptk_thread_has_signal(PTK_THREAD_SIGNAL_CHILD_DIED)) {
            completed_children++;
            info("Child completed (%d/%d)", completed_children, num_children);
            ptk_thread_clear_signals(PTK_THREAD_SIGNAL_CHILD_DIED);
        } else if (wait_result != PTK_ERR_SIGNAL) {
            error("Wait for children failed or timed out");
            break;
        }
    }
    
    // Clean up dead children
    ptk_thread_cleanup_dead_children(self, PTK_TIME_NO_WAIT);
    
    // Verify child count after cleanup
    int final_count = ptk_thread_count_children(self);
    info("Final child count: %d", final_count);
    
    if (final_count != initial_count) {
        error("Child count after cleanup incorrect: %d != %d", final_count, initial_count);
        // Clean up
        for (int i = 0; i < num_children; i++) {
            ptk_shared_release(thread_data_handles[i]);
            ptk_shared_release(children[i]);
        }
        ptk_shared_shutdown();
        return 5;
    }
    
    // Clean up
    for (int i = 0; i < num_children; i++) {
        ptk_shared_release(thread_data_handles[i]);
        ptk_shared_release(children[i]);
    }
    ptk_shared_shutdown();
    
    info("test_thread_parent_child_relationships exit");
    return 0;
}

int test_thread_signal_all_children(void) {
    info("test_thread_signal_all_children entry");
    
    // Initialize shared memory
    ptk_err_t err = ptk_shared_init();
    if (err != PTK_OK) {
        error("ptk_shared_init failed");
        return 1;
    }
    
    ptk_thread_handle_t self = ptk_thread_self();
    
    // Create multiple long-running child threads
    const int num_children = 2;
    ptk_thread_handle_t children[num_children];
    ptk_shared_handle_t thread_data_handles[num_children];
    
    for (int i = 0; i < num_children; i++) {
        thread_data_handles[i] = ptk_shared_alloc(sizeof(thread_test_data_t), NULL);
        if (!ptk_shared_is_valid(thread_data_handles[i])) {
            error("Failed to allocate thread data %d", i);
            // Clean up previous allocations
            for (int j = 0; j < i; j++) {
                ptk_shared_release(thread_data_handles[j]);
            }
            ptk_shared_shutdown();
            return 2;
        }
        
        thread_test_data_t *data = ptk_shared_acquire(thread_data_handles[i], PTK_TIME_WAIT_FOREVER);
        data->thread_id = i + 1;
        data->iterations = 100; // Long running
        data->should_signal_parent = false;
        data->result_handle = PTK_SHARED_INVALID_HANDLE;
        ptk_shared_release(thread_data_handles[i]);
        
        children[i] = ptk_thread_create();
        if (!ptk_shared_is_valid(children[i])) {
            error("Failed to create long-running child thread %d", i);
            // Clean up
            for (int j = 0; j <= i; j++) {
                ptk_shared_release(thread_data_handles[j]);
            }
            for (int j = 0; j < i; j++) {
                ptk_shared_release(children[j]);
            }
            ptk_shared_shutdown();
            return 3;
        }
        
        err = ptk_thread_add_handle_arg(children[i], 0, &thread_data_handles[i]);
        if (err != PTK_OK) {
            error("Failed to add handle arg to long-running child thread %d: %d", i, err);
            ptk_shared_release(children[i]);
            // Clean up
            for (int j = 0; j <= i; j++) {
                ptk_shared_release(thread_data_handles[j]);
            }
            for (int j = 0; j < i; j++) {
                ptk_shared_release(children[j]);
            }
            ptk_shared_shutdown();
            return 3;
        }
        
        err = ptk_thread_set_run_function(children[i], long_running_thread_func);
        if (err != PTK_OK) {
            error("Failed to set run function for long-running child thread %d: %d", i, err);
            ptk_shared_release(children[i]);
            // Clean up
            for (int j = 0; j <= i; j++) {
                ptk_shared_release(thread_data_handles[j]);
            }
            for (int j = 0; j < i; j++) {
                ptk_shared_release(children[j]);
            }
            ptk_shared_shutdown();
            return 3;
        }
        
        err = ptk_thread_start(children[i]);
        if (err != PTK_OK) {
            error("Failed to start long-running child thread %d: %d", i, err);
            ptk_shared_release(children[i]);
            // Clean up
            for (int j = 0; j <= i; j++) {
                ptk_shared_release(thread_data_handles[j]);
            }
            for (int j = 0; j < i; j++) {
                ptk_shared_release(children[j]);
            }
            ptk_shared_shutdown();
            return 3;
        }
    }
    
    info("Created %d long-running threads", num_children);
    
    // Let them run for a bit
    usleep(50000); // 50ms
    
    // Test ptk_thread_signal_all_children
    info("Signaling all children to terminate");
    ptk_thread_signal_all_children(self, PTK_THREAD_SIGNAL_TERMINATE);
    
    // Wait for all children to complete
    int completed_children = 0;
    while (completed_children < num_children) {
        ptk_err_t wait_result = ptk_thread_wait(5000);
        if (wait_result == PTK_ERR_SIGNAL && ptk_thread_has_signal(PTK_THREAD_SIGNAL_CHILD_DIED)) {
            completed_children++;
            info("Child terminated (%d/%d)", completed_children, num_children);
            ptk_thread_clear_signals(PTK_THREAD_SIGNAL_CHILD_DIED);
        } else if (wait_result != PTK_ERR_SIGNAL) {
            error("Wait for children termination failed or timed out");
            break;
        }
    }
    
    if (completed_children != num_children) {
        error("Not all children terminated: %d/%d", completed_children, num_children);
        // Clean up
        for (int i = 0; i < num_children; i++) {
            ptk_shared_release(thread_data_handles[i]);
            ptk_shared_release(children[i]);
        }
        ptk_shared_shutdown();
        return 4;
    }
    
    // Clean up
    ptk_thread_cleanup_dead_children(self, PTK_TIME_NO_WAIT);
    for (int i = 0; i < num_children; i++) {
        ptk_shared_release(thread_data_handles[i]);
        ptk_shared_release(children[i]);
    }
    ptk_shared_shutdown();
    
    info("test_thread_signal_all_children exit");
    return 0;
}

//=============================================================================
// Advanced Threading Tests
//=============================================================================

int test_thread_timeout_scenarios(void) {
    info("test_thread_timeout_scenarios entry");
    
    // Initialize shared memory
    ptk_err_t err = ptk_shared_init();
    if (err != PTK_OK) {
        error("ptk_shared_init failed");
        return 1;
    }
    
    // Test timeout with no children
    ptk_err_t wait_result = ptk_thread_wait(100); // 100ms timeout
    if (wait_result != PTK_OK) {
        info("Thread wait with no children timed out correctly");
    } else {
        error("Thread wait should have timed out");
        ptk_shared_shutdown();
        return 1;
    }
    
    // Test no-wait behavior
    wait_result = ptk_thread_wait(PTK_TIME_NO_WAIT);
    if (wait_result != PTK_OK) {
        info("Thread wait with no-wait returned correctly");
    } else {
        error("Thread wait with no-wait should not block");
        ptk_shared_shutdown();
        return 2;
    }
    
    ptk_shared_shutdown();
    
    info("test_thread_timeout_scenarios exit");
    return 0;
}

int test_thread_signal_combinations(void) {
    info("test_thread_signal_combinations entry");
    
    // Initialize shared memory
    ptk_err_t err = ptk_shared_init();
    if (err != PTK_OK) {
        error("ptk_shared_init failed");
        return 1;
    }
    
    ptk_thread_handle_t self = ptk_thread_self();
    
    // Test multiple signals at once (bitflags)
    ptk_thread_signal(self, PTK_THREAD_SIGNAL_WAKE);
    ptk_thread_signal(self, PTK_THREAD_SIGNAL_ABORT);
    
    // Check that both signals are present
    if (!ptk_thread_has_signal(PTK_THREAD_SIGNAL_WAKE)) {
        error("WAKE signal not present");
        ptk_shared_shutdown();
        return 1;
    }
    
    if (!ptk_thread_has_signal(PTK_THREAD_SIGNAL_ABORT)) {
        error("ABORT signal not present");
        ptk_shared_shutdown();
        return 2;
    }
    
    // Test getting pending signals
    uint64_t pending = ptk_thread_get_pending_signals();
    info("Pending signals: 0x%lx", pending);
    
    if (!(pending & PTK_THREAD_SIGNAL_WAKE)) {
        error("WAKE signal not in pending signals");
        ptk_shared_shutdown();
        return 3;
    }
    
    if (!(pending & PTK_THREAD_SIGNAL_ABORT)) {
        error("ABORT signal not in pending signals");
        ptk_shared_shutdown();
        return 4;
    }
    
    // Test clearing individual signals
    ptk_thread_clear_signals(PTK_THREAD_SIGNAL_WAKE);
    
    if (ptk_thread_has_signal(PTK_THREAD_SIGNAL_WAKE)) {
        error("WAKE signal not cleared");
        ptk_shared_shutdown();
        return 5;
    }
    
    if (!ptk_thread_has_signal(PTK_THREAD_SIGNAL_ABORT)) {
        error("ABORT signal should still be present");
        ptk_shared_shutdown();
        return 6;
    }
    
    // Test clearing multiple signals
    ptk_thread_signal(self, PTK_THREAD_SIGNAL_WAKE);
    ptk_thread_signal(self, PTK_THREAD_SIGNAL_TERMINATE);
    
    uint64_t clear_mask = PTK_THREAD_SIGNAL_WAKE | PTK_THREAD_SIGNAL_TERMINATE;
    ptk_thread_clear_signals(clear_mask);
    
    if (ptk_thread_has_signal(PTK_THREAD_SIGNAL_WAKE) || 
        ptk_thread_has_signal(PTK_THREAD_SIGNAL_TERMINATE)) {
        error("Multiple signals not cleared properly");
        ptk_shared_shutdown();
        return 7;
    }
    
    // ABORT should still be present
    if (!ptk_thread_has_signal(PTK_THREAD_SIGNAL_ABORT)) {
        error("ABORT signal should still be present after multi-clear");
        ptk_shared_shutdown();
        return 8;
    }
    
    // Clear all signals
    ptk_thread_clear_signals(PTK_THREAD_SIGNAL_ABORT);
    
    if (ptk_thread_has_signal(PTK_THREAD_SIGNAL_ABORT)) {
        error("ABORT signal not cleared");
        ptk_shared_shutdown();
        return 9;
    }
    
    ptk_shared_shutdown();
    
    info("test_thread_signal_combinations exit");
    return 0;
}

int test_thread_error_conditions(void) {
    info("test_thread_error_conditions entry");
    
    // Test operations with invalid handles
    ptk_err_t result = ptk_thread_signal(PTK_SHARED_INVALID_HANDLE, PTK_THREAD_SIGNAL_WAKE);
    if (result == PTK_OK) {
        error("ptk_thread_signal should fail with invalid handle");
        return 1;
    }
    
    // Test parent operations with invalid handles
    ptk_thread_handle_t invalid_parent = ptk_thread_get_parent(PTK_SHARED_INVALID_HANDLE);
    if (ptk_shared_is_valid(invalid_parent)) {
        error("ptk_thread_get_parent should return invalid handle for invalid input");
        return 2;
    }
    
    // Test child count with invalid handle
    int count = ptk_thread_count_children(PTK_SHARED_INVALID_HANDLE);
    if (count != 0) {
        error("ptk_thread_count_children should return 0 for invalid handle");
        return 3;
    }
    
    // Test signal all children with invalid handle
    ptk_thread_signal_all_children(PTK_SHARED_INVALID_HANDLE, PTK_THREAD_SIGNAL_WAKE);
    // This should handle the error gracefully
    
    // Test cleanup with invalid handle
    ptk_thread_cleanup_dead_children(PTK_SHARED_INVALID_HANDLE, PTK_TIME_NO_WAIT);
    // This should handle the error gracefully
    
    info("test_thread_error_conditions exit");
    return 0;
}

//=============================================================================
// Main Test Function
//=============================================================================

int test_ptk_os_thread_main(void) {
    info("=== Starting PTK OS Thread Tests ===");
    
    int result = 0;
    
    // Basic threading tests
    if ((result = test_thread_creation_and_self()) != 0) {
        error("test_thread_creation_and_self failed with code %d", result);
        return result;
    }
    
    if ((result = test_thread_signaling()) != 0) {
        error("test_thread_signaling failed with code %d", result);
        return result;
    }
    
    if ((result = test_thread_parent_child_relationships()) != 0) {
        error("test_thread_parent_child_relationships failed with code %d", result);
        return result;
    }
    
    if ((result = test_thread_signal_all_children()) != 0) {
        error("test_thread_signal_all_children failed with code %d", result);
        return result;
    }
    
    // Advanced threading tests
    if ((result = test_thread_timeout_scenarios()) != 0) {
        error("test_thread_timeout_scenarios failed with code %d", result);
        return result;
    }
    
    if ((result = test_thread_signal_combinations()) != 0) {
        error("test_thread_signal_combinations failed with code %d", result);
        return result;
    }
    
    if ((result = test_thread_error_conditions()) != 0) {
        error("test_thread_error_conditions failed with code %d", result);
        return result;
    }
    
    info("=== All PTK OS Thread Tests Passed ===");
    return 0;
}

int main(void) {
    return test_ptk_os_thread_main();
}