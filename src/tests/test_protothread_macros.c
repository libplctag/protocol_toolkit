/**
 * @file test_protothread_macros.c
 * @brief Tests for protothread convenience macros
 *
 * Tests the protothread macros and embedded pattern functionality.
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

/* ========================================================================
 * TEST APPLICATION CONTEXT
 * ======================================================================== */

typedef struct {
    ptk_pt_t pt; /**< Must be first field for embedded pattern */
    int test_value;
    bool pt_started;
    bool pt_yielded;
    bool pt_ended;
} test_app_context_t;

/* ========================================================================
 * TEST PROTOTHREAD FUNCTIONS
 * ======================================================================== */

static void test_protothread_basic(ptk_pt_t *pt) {
    test_app_context_t *app = (test_app_context_t *)pt;

    PT_BEGIN(pt);

    app->pt_started = true;
    app->test_value = 42;

    PT_YIELD(pt);

    app->pt_yielded = true;
    app->test_value = 100;

    PT_END(pt);

    app->pt_ended = true;
}

static void test_protothread_exit(ptk_pt_t *pt) {
    test_app_context_t *app = (test_app_context_t *)pt;

    PT_BEGIN(pt);

    app->pt_started = true;
    app->test_value = 123;

    if(app->test_value == 123) { PT_EXIT(pt); }

    // This should never be reached
    app->pt_ended = true;

    PT_END(pt);
}

/* ========================================================================
 * EMBEDDED PATTERN TESTS
 * ======================================================================== */

void test_embedded_pattern() {
    printf("\n🧪 Testing Embedded Pattern\n");
    printf("===========================\n");

    test_app_context_t app = {0};

    // Initialize protothread
    ptk_err_t result = ptk_protothread_init(&app.pt, test_protothread_basic);
    TEST_ASSERT(result == PTK_ERR_OK, "Protothread initialization should succeed");

    // Test that pt is first field (addresses should match)
    TEST_ASSERT((void *)&app == (void *)&app.pt, "Protothread should be first field");

    // Test initial state
    TEST_ASSERT(app.test_value == 0, "Initial test value should be 0");
    TEST_ASSERT(!app.pt_started, "PT should not be started initially");
    TEST_ASSERT(!app.pt_yielded, "PT should not be yielded initially");
    TEST_ASSERT(!app.pt_ended, "PT should not be ended initially");
}

void test_protothread_execution() {
    printf("\n🧪 Testing Protothread Execution\n");
    printf("================================\n");

    test_app_context_t app = {0};
    ptk_protothread_init(&app.pt, test_protothread_basic);

    // First run - should start and yield
    ptk_protothread_run(&app.pt);

    TEST_ASSERT(app.pt_started, "PT should be started after first run");
    TEST_ASSERT(app.test_value == 42, "Test value should be 42 after first run");
    TEST_ASSERT(!app.pt_yielded, "PT should not show yielded flag yet");
    TEST_ASSERT(!app.pt_ended, "PT should not be ended after first run");

    // Second run - should resume and end
    ptk_protothread_run(&app.pt);

    TEST_ASSERT(app.pt_yielded, "PT should show yielded flag after second run");
    TEST_ASSERT(app.test_value == 100, "Test value should be 100 after second run");
    TEST_ASSERT(app.pt_ended, "PT should be ended after second run");
}

void test_protothread_exit_behavior() {
    printf("\n🧪 Testing Protothread Exit\n");
    printf("===========================\n");

    test_app_context_t app = {0};
    ptk_protothread_init(&app.pt, test_protothread_exit);

    // Run - should start and exit
    ptk_protothread_run(&app.pt);

    TEST_ASSERT(app.pt_started, "PT should be started");
    TEST_ASSERT(app.test_value == 123, "Test value should be 123");
    TEST_ASSERT(!app.pt_ended, "PT should not reach end after exit");

    // PT should be reset after exit
    TEST_ASSERT(app.pt.lc == 0, "PT line continuation should be reset after exit");
}

/* ========================================================================
 * PROTOTHREAD MACRO TESTS
 * ======================================================================== */

void test_pt_init_macro() {
    printf("\n🧪 Testing PT_INIT Macro\n");
    printf("========================\n");

    ptk_pt_t pt;
    pt.magic = 0xDEADBEEF;  // Set to wrong value
    pt.lc = 999;            // Set to wrong value

    PT_INIT(&pt);

    TEST_ASSERT(pt.magic == PTK_PT_MAGIC, "PT_INIT should set correct magic");
    TEST_ASSERT(pt.lc == 0, "PT_INIT should reset line continuation");
}

void test_magic_number_validation() {
    printf("\n🧪 Testing Magic Number Validation\n");
    printf("==================================\n");

    test_app_context_t app = {0};
    ptk_protothread_init(&app.pt, test_protothread_basic);

    TEST_ASSERT(app.pt.magic == PTK_PT_MAGIC, "Magic number should be set correctly");

    // Test with corrupted magic
    app.pt.magic = 0xBADC0DE;

    // ptk_protothread_run should check magic and not crash
    ptk_protothread_run(&app.pt);  // This should be safe even with bad magic

    TEST_ASSERT(true, "Running with bad magic should not crash");
}

/* ========================================================================
 * BUFFER INTEGRATION TESTS
 * ======================================================================== */

void test_buffer_in_context() {
    printf("\n🧪 Testing Buffer in Application Context\n");
    printf("========================================\n");

    typedef struct {
        ptk_pt_t pt;
        ptk_buffer_t buffer;
        uint8_t data[256];
    } buffer_app_context_t;

    buffer_app_context_t app = {0};
    app.buffer = ptk_buffer_create(app.data, sizeof(app.data));

    TEST_ASSERT(app.buffer.data == app.data, "Buffer should point to embedded data");
    TEST_ASSERT(app.buffer.capacity == 256, "Buffer capacity should be correct");
    TEST_ASSERT(app.buffer.size == 0, "Buffer size should be 0 initially");

    // Test casting works correctly
    ptk_pt_t *pt_ptr = &app.pt;
    buffer_app_context_t *cast_app = (buffer_app_context_t *)pt_ptr;

    TEST_ASSERT(cast_app == &app, "Cast should give same address");
    TEST_ASSERT(cast_app->buffer.data == app.data, "Cast should preserve buffer data pointer");
}

/* ========================================================================
 * MAIN TEST RUNNER
 * ======================================================================== */

int main() {
    printf("Protocol Toolkit - Protothread Macro Tests\n");
    printf("===========================================\n");

    // Run all tests
    test_embedded_pattern();
    test_protothread_execution();
    test_protothread_exit_behavior();
    test_pt_init_macro();
    test_magic_number_validation();
    test_buffer_in_context();

    // Print results
    printf("\n📊 Test Results\n");
    printf("===============\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_run - tests_passed);

    if(tests_passed == tests_run) {
        printf("🎉 All protothread tests passed!\n");
        return 0;
    } else {
        printf("💥 Some protothread tests failed!\n");
        return 1;
    }
}
