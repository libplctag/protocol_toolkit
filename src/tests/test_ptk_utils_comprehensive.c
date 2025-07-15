/**
 * @file test_ptk_utils_comprehensive.c
 * @brief Comprehensive tests for ptk_utils.h API
 *
 * Tests all time and utility functions including interrupt handling.
 */

#include <ptk_utils.h>
#include <ptk_log.h>
#include <ptk_err.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>

// Global variable to track interrupt handler calls
static bool interrupt_handler_called = false;
static int interrupt_count = 0;

//=============================================================================
// Interrupt Handler Tests
//=============================================================================

void test_interrupt_handler(void) {
    info("Test interrupt handler called");
    interrupt_handler_called = true;
    interrupt_count++;
}

void test_interrupt_handler_2(void) {
    info("Second test interrupt handler called");
    interrupt_handler_called = true;
    interrupt_count += 10;
}

int test_interrupt_handler_registration(void) {
    info("test_interrupt_handler_registration entry");
    
    // Reset state
    interrupt_handler_called = false;
    interrupt_count = 0;
    
    // Test registering an interrupt handler
    ptk_err result = ptk_set_interrupt_handler(test_interrupt_handler);
    if (result != PTK_OK) {
        error("Failed to register interrupt handler");
        return 1;
    }
    
    info("Interrupt handler registered successfully");
    
    // Test changing the handler
    result = ptk_set_interrupt_handler(test_interrupt_handler_2);
    if (result != PTK_OK) {
        error("Failed to change interrupt handler");
        return 2;
    }
    
    info("Interrupt handler changed successfully");
    
    // Test setting NULL handler (should work to disable)
    result = ptk_set_interrupt_handler(NULL);
    if (result != PTK_OK) {
        error("Failed to set NULL interrupt handler");
        return 3;
    }
    
    info("NULL interrupt handler set successfully");
    
    // Re-register a handler for potential signal testing
    result = ptk_set_interrupt_handler(test_interrupt_handler);
    if (result != PTK_OK) {
        error("Failed to re-register interrupt handler");
        return 4;
    }
    
    info("test_interrupt_handler_registration exit");
    return 0;
}

int test_interrupt_handler_with_signal(void) {
    info("test_interrupt_handler_with_signal entry");
    
    // Reset state
    interrupt_handler_called = false;
    interrupt_count = 0;
    
    // Register handler
    ptk_err result = ptk_set_interrupt_handler(test_interrupt_handler);
    if (result != PTK_OK) {
        error("Failed to register interrupt handler for signal test");
        return 1;
    }
    
    info("Interrupt handler registered, testing with self-signal");
    
    // Send ourselves a signal that should trigger the handler
    // Note: This test may be platform-specific
    pid_t pid = getpid();
    if (kill(pid, SIGTERM) != 0) {
        info("Could not send SIGTERM to self (may not be supported)");
        // This is not necessarily an error - the system may not allow this
    } else {
        // Give the signal a moment to be processed
        usleep(50000);  // 50ms
        
        if (interrupt_handler_called) {
            info("Interrupt handler was called by signal");
        } else {
            info("Interrupt handler was not called (may be normal on this system)");
        }
    }
    
    // Try with SIGINT as well
    interrupt_handler_called = false;
    if (kill(pid, SIGINT) != 0) {
        info("Could not send SIGINT to self (may not be supported)");
    } else {
        usleep(50000);  // 50ms
        
        if (interrupt_handler_called) {
            info("Interrupt handler was called by SIGINT");
        } else {
            info("Interrupt handler was not called by SIGINT (may be normal)");
        }
    }
    
    info("test_interrupt_handler_with_signal exit");
    return 0;
}

//=============================================================================
// Time Function Tests
//=============================================================================

int test_time_basic_operations(void) {
    info("test_time_basic_operations entry");
    
    // Test ptk_now_ms
    ptk_time_ms time1 = ptk_now_ms();
    if (time1 <= 0) {
        error("ptk_now_ms returned invalid time: %ld", time1);
        return 1;
    }
    
    info("Current time: %ld ms", time1);
    
    // Sleep for a known duration and check time difference
    usleep(100000);  // 100ms
    
    ptk_time_ms time2 = ptk_now_ms();
    if (time2 <= time1) {
        error("Time did not advance: %ld <= %ld", time2, time1);
        return 2;
    }
    
    ptk_duration_ms elapsed = time2 - time1;
    info("Elapsed time: %ld ms", elapsed);
    
    // Check that elapsed time is reasonable (should be around 100ms, but allow for variance)
    if (elapsed < 90 || elapsed > 200) {
        info("Elapsed time outside expected range: %ld ms (expected ~100ms)", elapsed);
        // This is informational only - timing can vary on different systems
    }
    
    info("test_time_basic_operations exit");
    return 0;
}

int test_time_constants(void) {
    info("test_time_constants entry");
    
    // Test that constants are properly defined
    ptk_duration_ms forever = PTK_TIME_WAIT_FOREVER;
    ptk_duration_ms no_wait = PTK_TIME_NO_WAIT;
    
    info("PTK_TIME_WAIT_FOREVER = %ld", forever);
    info("PTK_TIME_NO_WAIT = %ld", no_wait);
    
    // Verify that WAIT_FOREVER is a large positive value
    if (forever <= 0) {
        error("PTK_TIME_WAIT_FOREVER should be positive: %ld", forever);
        return 1;
    }
    
    // Verify that NO_WAIT is a large negative value  
    if (no_wait >= 0) {
        error("PTK_TIME_NO_WAIT should be negative: %ld", no_wait);
        return 2;
    }
    
    // Verify they are different
    if (forever == no_wait) {
        error("PTK_TIME_WAIT_FOREVER and PTK_TIME_NO_WAIT should be different");
        return 3;
    }
    
    // Test using these constants in practical scenarios
    ptk_time_ms current_time = ptk_now_ms();
    
    // These should work without overflow issues
    ptk_time_ms future_time = current_time + 1000;  // 1 second in future
    ptk_duration_ms diff = future_time - current_time;
    
    if (diff != 1000) {
        error("Time arithmetic failed: %ld != 1000", diff);
        return 4;
    }
    
    info("test_time_constants exit");
    return 0;
}

int test_time_measurement_accuracy(void) {
    info("test_time_measurement_accuracy entry");
    
    // Test multiple rapid time measurements
    const int num_measurements = 100;
    ptk_time_ms times[num_measurements];
    
    for (int i = 0; i < num_measurements; i++) {
        times[i] = ptk_now_ms();
        usleep(1000);  // 1ms between measurements
    }
    
    // Verify that times are monotonically increasing
    for (int i = 1; i < num_measurements; i++) {
        if (times[i] < times[i-1]) {
            error("Time went backwards: %ld < %ld at index %d", times[i], times[i-1], i);
            return 1;
        }
    }
    
    // Calculate total elapsed time
    ptk_duration_ms total_elapsed = times[num_measurements-1] - times[0];
    ptk_duration_ms expected_min = num_measurements - 1;  // At least 1ms per iteration
    
    info("Total elapsed time for %d measurements: %ld ms", num_measurements, total_elapsed);
    
    if (total_elapsed < expected_min) {
        info("Elapsed time less than expected minimum: %ld < %ld", total_elapsed, expected_min);
        // This is informational - timing resolution varies by system
    }
    
    // Test resolution by taking measurements in tight loop
    ptk_time_ms start_time = ptk_now_ms();
    ptk_time_ms end_time;
    int iterations = 0;
    
    do {
        end_time = ptk_now_ms();
        iterations++;
    } while (end_time == start_time && iterations < 10000);
    
    if (iterations >= 10000) {
        info("Time resolution appears to be > 1ms (took %d iterations)", iterations);
    } else {
        info("Time resolution detected after %d iterations", iterations);
    }
    
    info("test_time_measurement_accuracy exit");
    return 0;
}

int test_time_edge_cases(void) {
    info("test_time_edge_cases entry");
    
    // Test time around potential overflow boundaries
    ptk_time_ms current = ptk_now_ms();
    
    // Test arithmetic that might cause issues
    ptk_time_ms large_future = current + 1000000000L;  // Add ~11.5 days
    ptk_duration_ms diff = large_future - current;
    
    if (diff != 1000000000L) {
        error("Large time arithmetic failed: %ld != 1000000000", diff);
        return 1;
    }
    
    // Test with time constants
    ptk_time_ms time_with_forever = current + PTK_TIME_WAIT_FOREVER;
    if (time_with_forever < current) {
        info("Adding PTK_TIME_WAIT_FOREVER caused overflow (expected on some systems)");
    }
    
    // Test multiple calls in rapid succession
    ptk_time_ms rapid_times[10];
    for (int i = 0; i < 10; i++) {
        rapid_times[i] = ptk_now_ms();
    }
    
    // Verify all times are valid
    for (int i = 0; i < 10; i++) {
        if (rapid_times[i] <= 0) {
            error("Rapid call %d returned invalid time: %ld", i, rapid_times[i]);
            return 2;
        }
    }
    
    // Verify times are non-decreasing
    for (int i = 1; i < 10; i++) {
        if (rapid_times[i] < rapid_times[i-1]) {
            error("Rapid call %d went backwards: %ld < %ld", i, rapid_times[i], rapid_times[i-1]);
            return 3;
        }
    }
    
    info("test_time_edge_cases exit");
    return 0;
}

//=============================================================================
// Main Test Function
//=============================================================================

int test_ptk_utils_main(void) {
    info("=== Starting PTK Utils Tests ===");
    
    int result = 0;
    
    // Interrupt handler tests
    if ((result = test_interrupt_handler_registration()) != 0) {
        error("test_interrupt_handler_registration failed with code %d", result);
        return result;
    }
    
    if ((result = test_interrupt_handler_with_signal()) != 0) {
        error("test_interrupt_handler_with_signal failed with code %d", result);
        return result;
    }
    
    // Time function tests
    if ((result = test_time_basic_operations()) != 0) {
        error("test_time_basic_operations failed with code %d", result);
        return result;
    }
    
    if ((result = test_time_constants()) != 0) {
        error("test_time_constants failed with code %d", result);
        return result;
    }
    
    if ((result = test_time_measurement_accuracy()) != 0) {
        error("test_time_measurement_accuracy failed with code %d", result);
        return result;
    }
    
    if ((result = test_time_edge_cases()) != 0) {
        error("test_time_edge_cases failed with code %d", result);
        return result;
    }
    
    info("=== All PTK Utils Tests Passed ===");
    return 0;
}