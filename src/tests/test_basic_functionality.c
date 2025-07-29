/**
 * @file test_basic_functionality.c
 * @brief Basic functionality tests for Protocol Toolkit
 *
 * Tests core functionality like handle creation, buffer operations,
 * and basic resource management.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#ifdef __APPLE__
#    include "../include/macos/protocol_toolkit.h"
#elif __linux__
#    include "../lib/linux/protocol_toolkit.h"
#else
#    include "../include/protocol_toolkit.h"
#endif

/* ========================================================================
 * TEST UTILITIES
 * ======================================================================== */

static int tests_run = 0;
static int tests_passed = 0;

#define TEST_ASSERT(condition, message)       \
    do {                                      \
        tests_run++;                          \
        if(condition) {                       \
            tests_passed++;                   \
            printf("✅ PASS: %s\n", message); \
        } else {                              \
            printf("❌ FAIL: %s\n", message); \
        }                                     \
    } while(0)

// Dummy function for protothread testing
static void dummy_protothread_func(ptk_pt_t *p) { (void)p; }

/* ========================================================================
 * BUFFER TESTS
 * ======================================================================== */

void test_buffer_creation() {
    printf("\n🧪 Testing Buffer Creation\n");
    printf("==========================\n");

    uint8_t data[100];
    ptk_buffer_t buffer = ptk_buffer_create(data, sizeof(data));

    TEST_ASSERT(buffer.data == data, "Buffer data pointer should match");
    TEST_ASSERT(buffer.size == 0, "Buffer size should be 0 initially");
    TEST_ASSERT(buffer.capacity == 100, "Buffer capacity should be 100");
}

void test_buffer_operations() {
    printf("\n🧪 Testing Buffer Operations\n");
    printf("============================\n");

    uint8_t data[50];
    ptk_buffer_t buffer = ptk_buffer_create(data, sizeof(data));

    // Test writing data
    const char *test_string = "Hello, World!";
    size_t test_len = strlen(test_string);

    if(test_len <= buffer.capacity) {
        memcpy(buffer.data, test_string, test_len);
        buffer.size = test_len;

        TEST_ASSERT(buffer.size == test_len, "Buffer size should match written data");
        TEST_ASSERT(memcmp(buffer.data, test_string, test_len) == 0, "Buffer data should match written string");
    }

    TEST_ASSERT(buffer.capacity == 50, "Buffer capacity should remain unchanged");
}

/* ========================================================================
 * HANDLE TESTS
 * ======================================================================== */

void test_handle_macros() {
    printf("\n🧪 Testing Handle Macros\n");
    printf("========================\n");

    // Create a test handle
    ptk_handle_t handle = PTK_MAKE_HANDLE(PTK_TYPE_SOCKET, 5, 123, 456789);

    TEST_ASSERT(PTK_HANDLE_TYPE(handle) == PTK_TYPE_SOCKET, "Handle type should be SOCKET");
    TEST_ASSERT(PTK_HANDLE_EVENT_LOOP_ID(handle) == 5, "Event loop ID should be 5");
    TEST_ASSERT(PTK_HANDLE_GENERATION(handle) == 123, "Generation should be 123");
    TEST_ASSERT(PTK_HANDLE_ID(handle) == 456789, "Handle ID should be 456789");
}

void test_handle_validation() {
    printf("\n🧪 Testing Handle Validation\n");
    printf("============================\n");

    ptk_handle_t valid_handle = PTK_MAKE_HANDLE(PTK_TYPE_TIMER, 1, 1, 1);
    ptk_handle_t invalid_handle = 0;

    TEST_ASSERT(ptk_handle_is_valid(valid_handle), "Valid handle should be valid");
    TEST_ASSERT(!ptk_handle_is_valid(invalid_handle), "Invalid handle should be invalid");
    TEST_ASSERT(ptk_handle_get_type(valid_handle) == PTK_TYPE_TIMER, "Handle type should be extracted correctly");
}

/* ========================================================================
 * PROTOTHREAD TESTS
 * ======================================================================== */

void test_protothread_initialization() {
    printf("\n🧪 Testing Protothread Initialization\n");
    printf("=====================================\n");

    ptk_pt_t pt;

    // Test initialization
    ptk_err_t result = ptk_protothread_init(&pt, NULL);
    TEST_ASSERT(result == PTK_ERR_INVALID_ARGUMENT, "Init with NULL function should fail");

    result = ptk_protothread_init(&pt, dummy_protothread_func);
    TEST_ASSERT(result == PTK_ERR_OK, "Init with valid function should succeed");
    TEST_ASSERT(pt.magic == PTK_PT_MAGIC, "Magic number should be set");
    TEST_ASSERT(pt.lc == 0, "Line continuation should be 0");
    TEST_ASSERT(pt.function == dummy_protothread_func, "Function pointer should be set");
}

/* ========================================================================
 * EVENT LOOP TESTS
 * ======================================================================== */

void test_event_loop_creation() {
    printf("\n🧪 Testing Event Loop Creation\n");
    printf("==============================\n");

    // Declare resources
    PTK_DECLARE_EVENT_LOOP_SLOTS(test_loops, 2);
    PTK_DECLARE_EVENT_LOOP_RESOURCES(test_resources, 4, 8, 2);

    // Test creation
    ptk_handle_t loop = ptk_event_loop_create(test_loops, 2, &test_resources);
    TEST_ASSERT(loop != 0, "Event loop creation should succeed");
    TEST_ASSERT(ptk_handle_is_valid(loop), "Created event loop handle should be valid");
    TEST_ASSERT(ptk_handle_get_type(loop) == PTK_TYPE_EVENT_LOOP, "Handle type should be EVENT_LOOP");

    // Test destruction
    ptk_err_t result = ptk_event_loop_destroy(loop);
    TEST_ASSERT(result == PTK_ERR_OK, "Event loop destruction should succeed");
}

/* ========================================================================
 * ERROR HANDLING TESTS
 * ======================================================================== */

void test_error_strings() {
    printf("\n🧪 Testing Error Strings\n");
    printf("========================\n");

    const char *msg = ptk_error_string(PTK_ERR_OK);
    TEST_ASSERT(msg != NULL, "Error string should not be NULL");
    if(msg != NULL) { TEST_ASSERT(strlen(msg) > 0, "Error string should not be empty"); }

    msg = ptk_error_string(PTK_ERR_INVALID_HANDLE);
    TEST_ASSERT(msg != NULL, "Invalid handle error string should not be NULL");

    msg = ptk_error_string(PTK_ERR_NETWORK_ERROR);
    TEST_ASSERT(msg != NULL, "Network error string should not be NULL");
}

/* ========================================================================
 * MAIN TEST RUNNER
 * ======================================================================== */

int main() {
    printf("Protocol Toolkit - Basic Functionality Tests\n");
    printf("=============================================\n");

    // Run all tests
    test_buffer_creation();
    test_buffer_operations();
    test_handle_macros();
    test_handle_validation();
    test_protothread_initialization();
    test_event_loop_creation();
    test_error_strings();

    // Print results
    printf("\n📊 Test Results\n");
    printf("===============\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_run - tests_passed);

    if(tests_passed == tests_run) {
        printf("🎉 All tests passed!\n");
        return 0;
    } else {
        printf("💥 Some tests failed!\n");
        return 1;
    }
}
