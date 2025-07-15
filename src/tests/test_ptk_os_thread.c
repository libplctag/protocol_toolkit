/**
 * @file test_ptk_os_thread.c
 * @brief Tests for ptk_os_thread API
 *
 * This file tests mutex and thread creation. Logging uses ptk_log.h, not ptk_os_thread.h except for the functions under test.
 */
#include <ptk_os_thread.h>
#include <ptk_mem.h>
#include <ptk_log.h>
#include <ptk_utils.h>
#include <stdio.h>

static int thread_ran = 0;

void thread_entry(ptk_shared_handle_t param) {
    info("thread_entry called");
    thread_ran = 1;
}

int test_thread_create() {
    info("test_thread_create entry");
    thread_ran = 0;
    
    // Create shared data for thread (empty in this case)
    ptk_shared_handle_t data = ptk_shared_alloc(sizeof(int), NULL);
    if (!PTK_SHARED_IS_VALID(data)) {
        error("Failed to allocate shared data for thread");
        return 1;
    }
    
    // Create thread with no parent (root thread)
    ptk_thread_handle_t th = ptk_thread_create(PTK_THREAD_NO_PARENT, thread_entry, data);
    if(!PTK_SHARED_IS_VALID(th)) {
        error("ptk_thread_create failed");
        ptk_shared_release(data);
        return 2;
    }
    
    // Wait for thread to complete by waiting for signals
    ptk_err result = ptk_thread_wait(PTK_TIME_WAIT_FOREVER);
    if(result == PTK_ERR_SIGNAL) {
        if (ptk_thread_has_signal(PTK_THREAD_SIGNAL_CHILD_DIED)) {
            info("Received child death signal as expected");
            ptk_thread_clear_signals(PTK_THREAD_SIGNAL_CHILD_DIED);
        } else {
            error("Expected child death signal, got 0x%lx", ptk_thread_get_pending_signals());
            ptk_shared_release(th);
            ptk_shared_release(data);
            return 3;
        }
    } else {
        error("ptk_thread_wait should have received child death signal");
        ptk_shared_release(th);
        ptk_shared_release(data);
        return 4;
    }
    
    if(!thread_ran) {
        error("thread did not run");
        ptk_shared_release(th);
        ptk_shared_release(data);
        return 5;
    }
    
    // Clean up dead children
    ptk_thread_cleanup_dead_children(ptk_thread_self(), PTK_TIME_NO_WAIT);
    
    // Release handles - automatic cleanup via destructors
    ptk_shared_release(th);
    ptk_shared_release(data);
    
    info("test_thread_create exit");
    return 0;
}

int test_thread_signal() {
    info("test_thread_signal entry");
    
    ptk_thread_handle_t self = ptk_thread_self();
    
    // Create shared data for thread communication test
    ptk_shared_handle_t signal_data = ptk_shared_alloc(sizeof(int), NULL);
    if (!PTK_SHARED_IS_VALID(signal_data)) {
        error("Failed to allocate shared data for signal test");
        return 1;
    }
    
    // Initialize signal flag
    int *flag = ptk_shared_acquire(signal_data, PTK_TIME_WAIT_FOREVER);
    if (!flag) {
        error("Failed to acquire signal data");
        ptk_shared_release(signal_data);
        return 2;
    }
    *flag = 0;  // Not signaled
    ptk_shared_release(signal_data);
    
    // Create a child thread
    ptk_thread_handle_t thread = ptk_thread_create(self, thread_entry, signal_data);
    if (!PTK_SHARED_IS_VALID(thread)) {
        error("Failed to create child thread for signal test");
        ptk_shared_release(signal_data);
        return 3;
    }
    
    // Test signaling the thread with different signal types
    ptk_err result = ptk_thread_signal(thread, PTK_THREAD_SIGNAL_WAKE);
    if (result != PTK_OK) {
        error("ptk_thread_signal failed");
        ptk_shared_release(thread);
        ptk_shared_release(signal_data);
        return 4;
    }
    
    // Wait for child to die
    result = ptk_thread_wait(2000);  // 2 second timeout
    if (result == PTK_ERR_SIGNAL) {
        if (ptk_thread_has_signal(PTK_THREAD_SIGNAL_CHILD_DIED)) {
            info("Received child death notification as expected");
            ptk_thread_clear_signals(PTK_THREAD_SIGNAL_CHILD_DIED);
        } else {
            error("Expected child death signal, got 0x%lx", ptk_thread_get_pending_signals());
            ptk_shared_release(thread);
            ptk_shared_release(signal_data);
            return 5;
        }
    } else {
        error("Should have received child death signal");
        ptk_shared_release(thread);
        ptk_shared_release(signal_data);
        return 6;
    }
    
    // Clean up dead children
    ptk_thread_cleanup_dead_children(self, PTK_TIME_NO_WAIT);
    
    // Check child count
    int child_count = ptk_thread_count_children(self);
    if (child_count != 0) {
        error("Child count should be 0 after cleanup, got %d", child_count);
        ptk_shared_release(thread);
        ptk_shared_release(signal_data);
        return 7;
    }
    
    // Clean up
    ptk_shared_release(thread);
    ptk_shared_release(signal_data);
    
    info("test_thread_signal exit");
    return 0;
}

int main() {
    int result = test_thread_create();
    if(result == 0) {
        info("ptk_os_thread thread creation test PASSED");
    } else {
        error("ptk_os_thread thread creation test FAILED");
        return result;
    }
    
    result = test_thread_signal();
    if(result == 0) {
        info("ptk_os_thread thread signal test PASSED");
    } else {
        error("ptk_os_thread thread signal test FAILED");
        return result;
    }
    
    info("All ptk_os_thread tests PASSED");
    return 0;
}
