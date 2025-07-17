/**
 * @file test_ptk_log_comprehensive.c
 * @brief Comprehensive tests for ptk_log.h API
 *
 * Tests all logging functionality including level management, buffer logging,
 * and all logging macros.
 */

#include <ptk_log.h>
#include <ptk_err.h>
#include <ptk_buf.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

//=============================================================================
// Log Level Management Tests
//=============================================================================

int test_log_level_operations(void) {
    info("test_log_level_operations entry");
    
    // Test getting default log level
    ptk_log_level current_level = ptk_log_level_get();
    info("Default log level: %d", current_level);
    
    // Test setting different log levels
    ptk_log_level test_levels[] = {
        PTK_LOG_LEVEL_TRACE,
        PTK_LOG_LEVEL_DEBUG,
        PTK_LOG_LEVEL_INFO,
        PTK_LOG_LEVEL_WARN,
        PTK_LOG_LEVEL_ERROR
    };
    
    for (size_t i = 0; i < sizeof(test_levels)/sizeof(test_levels[0]); i++) {
        ptk_log_level_set(test_levels[i]);
        ptk_log_level retrieved = ptk_log_level_get();
        
        if (retrieved != test_levels[i]) {
            error("Log level not set correctly: expected %d, got %d", test_levels[i], retrieved);
            return 1;
        }
        
        info("Successfully set log level to %d", test_levels[i]);
    }
    
    // Restore original level
    ptk_log_level_set(current_level);
    
    info("test_log_level_operations exit");
    return 0;
}

int test_log_level_filtering(void) {
    info("test_log_level_filtering entry");
    
    // Set to INFO level
    ptk_log_level_set(PTK_LOG_LEVEL_INFO);
    
    info("This INFO message should appear");
    warn("This WARN message should appear");
    error("This ERROR message should appear");
    
    // Note: DEBUG and TRACE messages won't appear, but we can't easily test
    // their absence without redirecting output
    debug("This DEBUG message should NOT appear");
    trace("This TRACE message should NOT appear");
    
    // Set to ERROR level
    ptk_log_level_set(PTK_LOG_LEVEL_ERROR);
    
    error("This ERROR message should appear");
    info("This INFO message should NOT appear");
    warn("This WARN message should NOT appear");
    
    // Set to TRACE level (most verbose)
    ptk_log_level_set(PTK_LOG_LEVEL_TRACE);
    
    trace("This TRACE message should appear");
    debug("This DEBUG message should appear");
    info("This INFO message should appear");
    warn("This WARN message should appear");
    error("This ERROR message should appear");
    
    // Reset to INFO
    ptk_log_level_set(PTK_LOG_LEVEL_INFO);
    
    info("test_log_level_filtering exit");
    return 0;
}

//=============================================================================
// Basic Logging Macro Tests
//=============================================================================

int test_all_logging_macros(void) {
    info("test_all_logging_macros entry");
    
    // Set to TRACE level to see all messages
    ptk_log_level_set(PTK_LOG_LEVEL_TRACE);
    
    // Test all logging macros with simple messages
    trace("TRACE: This is a trace message");
    debug("DEBUG: This is a debug message");
    info("INFO: This is an info message");
    warn("WARN: This is a warning message");
    error("ERROR: This is an error message");
    
    // Test with formatted messages
    int test_value = 42;
    const char *test_string = "test";
    
    trace("TRACE: Formatted message with int %d and string %s", test_value, test_string);
    debug("DEBUG: Formatted message with int %d and string %s", test_value, test_string);
    info("INFO: Formatted message with int %d and string %s", test_value, test_string);
    warn("WARN: Formatted message with int %d and string %s", test_value, test_string);
    error("ERROR: Formatted message with int %d and string %s", test_value, test_string);
    
    // Test with complex formatting
    float float_val = 3.14159f;
    unsigned long long_val = 1000000ULL;
    
    info("Complex formatting: float=%.2f, unsigned=%u, long=%llu", 
         float_val, 123U, long_val);
    
    // Test with empty format string
    info("");
    
    // Test with very long messages
    char long_message[512];
    for (int i = 0; i < 500; i++) {
        long_message[i] = 'A' + (i % 26);
    }
    long_message[500] = '\0';
    
    info("Long message test: %s", long_message);
    
    info("test_all_logging_macros exit");
    return 0;
}

//=============================================================================
// Buffer Logging Tests
//=============================================================================

int test_buffer_logging(void) {
    info("test_buffer_logging entry");
    
    // Create a test buffer with various data
    ptk_buf *test_buf = ptk_buf_alloc(256);
    if (!test_buf) {
        error("Failed to allocate test buffer");
        return 1;
    }
    
    // Fill buffer with test pattern
    uint8_t test_data[] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F
    };
    
    for (size_t i = 0; i < sizeof(test_data); i++) {
        ptk_buf_set_u8(test_buf, test_data[i]);
    }
    ptk_buf_set_end(test_buf, sizeof(test_data));

    // check the size of the contents of the buffer
    size_t buf_size = ptk_buf_get_len(test_buf);

    if(buf_size != sizeof(test_data)) {
        error("Buffer size mismatch: expected %zu, got %zu", sizeof(test_data), buf_size);
        ptk_local_free(&test_buf);
        return 2;
    }
    
    // Test buffer logging at different levels
    trace_buf(test_buf);
    debug_buf(test_buf);
    info_buf(test_buf);
    warn_buf(test_buf);
    error_buf(test_buf);

    ptk_local_free(&test_buf);

    // Test with empty buffer
    ptk_buf *empty_buf = ptk_buf_alloc(100);
    if(!empty_buf) {
        error("Failed to allocate empty buffer");
        ptk_local_free(&test_buf);
        return 1;
    }

    info_buf(empty_buf);

    ptk_local_free(&empty_buf);

    // Test with single byte buffer
    ptk_buf *single_buf = ptk_buf_alloc(1);
    if(!single_buf) {
        error("Failed to allocate single byte buffer");
        ptk_local_free(&test_buf);
        return 1;
    }

    ptk_buf_set_u8(single_buf, 0xAB);
    if(ptk_get_err() != PTK_OK) {
        error("Failed to set single byte in buffer");
        ptk_local_free(&single_buf);
        ptk_local_free(&test_buf);
        return 1;
    }

    info_buf(single_buf);

    ptk_local_free(&single_buf);
    
    // Test with large buffer (should be truncated in display)
    ptk_buf *large_buf = ptk_buf_alloc(1024);
    if (!large_buf) {
        error("Failed to allocate large buffer");
        ptk_local_free(&large_buf);
        return 1;
    }

    for (int i = 0; i < 1024; i++) {
        ptk_buf_set_u8(large_buf, (uint8_t)(i & 0xFF));
    }

    info_buf(large_buf);

    ptk_local_free(&large_buf);

    // Test with NULL buffer (should handle gracefully)
    info_buf(NULL);
    
    info("test_buffer_logging exit");
    return 0;
}

//=============================================================================
// Direct Logging Implementation Tests
//=============================================================================

int test_direct_logging_implementation(void) {
    info("test_direct_logging_implementation entry");
    
    // Test ptk_log_impl directly
    ptk_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_DEBUG, "Direct debug message with int %d", 456);
    ptk_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_INFO, "Direct ptk_log_impl test");
    ptk_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_WARN, "Direct warning with format: %d", 123);
    ptk_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_ERROR, "Direct error with multiple args: %s %d %f", "test", 42, 3.14);
    // Test buffer logging implementation directly
    ptk_buf *test_buf = ptk_buf_alloc(16);
    if (test_buf) {
        uint8_t pattern[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE};
        for (size_t i = 0; i < sizeof(pattern); i++) {
            ptk_buf_set_u8(test_buf, pattern[i]);
        }
        ptk_buf_set_end(test_buf, sizeof(pattern));

        ptk_log_buf_impl(__func__, __LINE__, PTK_LOG_LEVEL_INFO, test_buf);
        ptk_local_free(&test_buf);
    }
    
    info("test_direct_logging_implementation exit");
    return 0;
}

//=============================================================================
// Edge Case Tests
//=============================================================================

int test_logging_edge_cases(void) {
    info("test_logging_edge_cases entry");
    
    // Test with NULL format string (should handle gracefully)
    // Note: Can't actually pass NULL to macros, but we can test empty strings
    info("");
    warn("");
    error("");
    
    // Test with special characters
    info("Special characters: \\n\\t\\r\\\" ' % %% \\\\");
    
    // Test with all format specifiers
    info("Format test: %d %i %o %x %X %f %F %e %E %g %G %c %s %p %%", 
         42, -42, 42, 42, 42, 3.14, 3.14, 3.14, 3.14, 3.14, 3.14, 'A', "test", (void*)&test_logging_edge_cases);
    
    // Test with very long format strings
    char long_fmt[256];
    strcpy(long_fmt, "Very long format string: ");
    for (int i = 0; i < 10; i++) {
        strcat(long_fmt, "item%d=%d ");
    }
    
    info(long_fmt, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10);
    
    // Test logging at different levels when level is set high
    ptk_log_level_set(PTK_LOG_LEVEL_ERROR);
    
    trace("This trace should not appear");
    debug("This debug should not appear");
    info("This info should not appear");
    warn("This warn should not appear");
    error("This error should appear");
    
    // Reset to INFO
    ptk_log_level_set(PTK_LOG_LEVEL_INFO);
    
    info("test_logging_edge_cases exit");
    return 0;
}

//=============================================================================
// Performance and Stress Tests
//=============================================================================

int test_logging_performance(void) {
    info("test_logging_performance entry");
    
    // Test rapid logging
    info("Starting rapid logging test...");
    for (int i = 0; i < 100; i++) {
        if (i % 10 == 0) {
            info("Rapid log iteration %d", i);
        } else {
            debug("Debug message %d", i);
        }
    }
    
    // Test logging at disabled level (should be fast)
    ptk_log_level_set(PTK_LOG_LEVEL_ERROR);
    
    info("Starting disabled level logging test...");
    for (int i = 0; i < 1000; i++) {
        trace("This trace message should be filtered out: %d", i);
        debug("This debug message should be filtered out: %d", i);
        info("This info message should be filtered out: %d", i);
    }
    
    // Reset level
    ptk_log_level_set(PTK_LOG_LEVEL_INFO);
    
    info("test_logging_performance exit");
    return 0;
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

int test_logging_thread_safety(void) {
    info("test_logging_thread_safety entry");
    
    // Test concurrent logging from multiple contexts
    // Note: This is a basic test - full thread safety would require
    // actual threading which is complex to test here
    
    info("Thread safety test - logging from different contexts");
    
    // Simulate different thread contexts by changing log levels
    ptk_log_level_set(PTK_LOG_LEVEL_TRACE);
    trace("Message from 'thread 1'");
    
    ptk_log_level_set(PTK_LOG_LEVEL_DEBUG);
    debug("Message from 'thread 2'");
    
    ptk_log_level_set(PTK_LOG_LEVEL_INFO);
    info("Message from 'thread 3'");
    
    ptk_log_level_set(PTK_LOG_LEVEL_WARN);
    warn("Message from 'thread 4'");
    
    ptk_log_level_set(PTK_LOG_LEVEL_ERROR);
    error("Message from 'thread 5'");
    
    // Reset to INFO
    ptk_log_level_set(PTK_LOG_LEVEL_INFO);
    
    // Test rapid level changes
    for (int i = 0; i < 10; i++) {
        ptk_log_level_set(PTK_LOG_LEVEL_TRACE + (i % 5));
        info("Level change test %d", i);
    }
    
    ptk_log_level_set(PTK_LOG_LEVEL_INFO);
    
    info("test_logging_thread_safety exit");
    return 0;
}

//=============================================================================
// Main Test Function
//=============================================================================

int test_ptk_log_main(void) {
    info("=== Starting PTK Logging Tests ===");
    
    int result = 0;
    
    // Log level management tests
    if ((result = test_log_level_operations()) != 0) {
        error("test_log_level_operations failed with code %d", result);
        return result;
    }
    
    if ((result = test_log_level_filtering()) != 0) {
        error("test_log_level_filtering failed with code %d", result);
        return result;
    }
    
    // Basic logging tests
    if ((result = test_all_logging_macros()) != 0) {
        error("test_all_logging_macros failed with code %d", result);
        return result;
    }
    
    // Buffer logging tests
    if ((result = test_buffer_logging()) != 0) {
        error("test_buffer_logging failed with code %d", result);
        return result;
    }
    
    // Direct implementation tests
    if ((result = test_direct_logging_implementation()) != 0) {
        error("test_direct_logging_implementation failed with code %d", result);
        return result;
    }
    
    // Edge case tests
    if ((result = test_logging_edge_cases()) != 0) {
        error("test_logging_edge_cases failed with code %d", result);
        return result;
    }
    
    // Performance tests
    if ((result = test_logging_performance()) != 0) {
        error("test_logging_performance failed with code %d", result);
        return result;
    }
    
    // Thread safety tests
    if ((result = test_logging_thread_safety()) != 0) {
        error("test_logging_thread_safety failed with code %d", result);
        return result;
    }
    
    info("=== All PTK Logging Tests Passed ===");
    return 0;
}

int main(void) {
    return test_ptk_log_main();
}