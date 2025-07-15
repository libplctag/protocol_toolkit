/**
 * @file test_harness.c
 * @brief Protocol Toolkit Test Harness
 *
 * Runs all Protocol Toolkit tests and reports results.
 */

#include <ptk_log.h>
#include <ptk_err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Test function declarations
extern int test_ptk_array_main(void);
extern int test_ptk_atomic_main(void);
extern int test_ptk_buf_main(void);
extern int test_ptk_config_main(void);
extern int test_ptk_err_main(void);
extern int test_ptk_log_main(void);
extern int test_ptk_mem_main(void);
extern int test_ptk_os_thread_main(void);
extern int test_ptk_sock_main(void);
extern int test_ptk_utils_main(void);

typedef struct {
    const char *name;
    int (*test_func)(void);
    const char *description;
} test_entry_t;

static const test_entry_t test_suite[] = {
    {"ptk_err", test_ptk_err_main, "Error handling API (comprehensive)"},
    {"ptk_utils", test_ptk_utils_main, "Time and utility functions (comprehensive)"},
    {"ptk_config", test_ptk_config_main, "Configuration parsing API (comprehensive)"},
    {"ptk_log", test_ptk_log_main, "Logging API (comprehensive)"},
    {"ptk_mem", test_ptk_mem_main, "Memory management API (comprehensive)"},
    {"ptk_atomic", test_ptk_atomic_main, "Atomic operations API (comprehensive)"},
    {"ptk_array", test_ptk_array_main, "Dynamic array API (comprehensive)"},
    {"ptk_buf", test_ptk_buf_main, "Buffer management and serialization API (comprehensive)"},
    {"ptk_os_thread", test_ptk_os_thread_main, "Threading and synchronization API (comprehensive)"},
    {"ptk_sock", test_ptk_sock_main, "Socket and networking API (comprehensive)"},
    {NULL, NULL, NULL}  // Sentinel
};

static void print_usage(const char *program_name) {
    printf("Usage: %s [options] [test_name]\n", program_name);
    printf("\nOptions:\n");
    printf("  -h, --help     Show this help message\n");
    printf("  -l, --list     List all available tests\n");
    printf("  -v, --verbose  Enable verbose output\n");
    printf("\nTest names:\n");
    for (int i = 0; test_suite[i].name; i++) {
        printf("  %-15s %s\n", test_suite[i].name, test_suite[i].description);
    }
    printf("\nIf no test name is specified, all tests will be run.\n");
}

static void list_tests(void) {
    printf("Available tests:\n");
    for (int i = 0; test_suite[i].name; i++) {
        printf("  %-15s %s\n", test_suite[i].name, test_suite[i].description);
    }
}

static int run_single_test(const test_entry_t *test, bool verbose) {
    printf("Running %s tests... ", test->name);
    fflush(stdout);
    
    if (verbose) {
        printf("\n=== %s: %s ===\n", test->name, test->description);
    }
    
    int result = test->test_func();
    
    if (result == 0) {
        printf("PASSED\n");
    } else {
        printf("FAILED (code %d)\n", result);
    }
    
    if (verbose) {
        printf("=== %s complete ===\n\n", test->name);
    }
    
    return result;
}

static int run_all_tests(bool verbose) {
    int passed = 0;
    int failed = 0;
    int total = 0;
    
    printf("Protocol Toolkit Test Suite\n");
    printf("===========================\n\n");
    
    for (int i = 0; test_suite[i].name; i++) {
        total++;
        int result = run_single_test(&test_suite[i], verbose);
        if (result == 0) {
            passed++;
        } else {
            failed++;
        }
    }
    
    printf("\n=== Test Summary ===\n");
    printf("Total tests: %d\n", total);
    printf("Passed: %d\n", passed);
    printf("Failed: %d\n", failed);
    printf("Success rate: %.1f%%\n", total > 0 ? (100.0 * passed / total) : 0.0);
    
    return failed > 0 ? 1 : 0;
}

static int run_specific_test(const char *test_name, bool verbose) {
    for (int i = 0; test_suite[i].name; i++) {
        if (strcmp(test_suite[i].name, test_name) == 0) {
            printf("Running specific test: %s\n", test_name);
            int result = run_single_test(&test_suite[i], verbose);
            return result;
        }
    }
    
    printf("Error: Test '%s' not found.\n", test_name);
    printf("Use --list to see available tests.\n");
    return 1;
}

int main(int argc, char *argv[]) {
    bool verbose = false;
    const char *specific_test = NULL;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--list") == 0) {
            list_tests();
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        } else if (argv[i][0] != '-') {
            if (specific_test) {
                printf("Error: Multiple test names specified.\n");
                return 1;
            }
            specific_test = argv[i];
        } else {
            printf("Error: Unknown option '%s'\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }
    
    // Set log level based on verbosity
    if (verbose) {
        ptk_log_level_set(PTK_LOG_DEBUG);
    } else {
        ptk_log_level_set(PTK_LOG_INFO);
    }
    
    // Run tests
    if (specific_test) {
        return run_specific_test(specific_test, verbose);
    } else {
        return run_all_tests(verbose);
    }
}