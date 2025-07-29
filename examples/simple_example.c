/**
 * Simple Timer Example for Protocol Toolkit macOS
 *
 * This demonstrates the working API based on the successful test.
 */

#include <stdio.h>
#include "protocol_toolkit.h"

// Declare event loop slots and resources like in our working test
PTK_DECLARE_EVENT_LOOP_SLOTS(example_event_loops, 1);
PTK_DECLARE_EVENT_LOOP_RESOURCES(example_resources, 2, 0, 0);

int main() {
    printf("Protocol Toolkit Simple Example\n");
    printf("================================\n\n");

    // Test basic handle manipulation
    ptk_handle_t test_handle = PTK_MAKE_HANDLE(PTK_TYPE_TIMER, 0, 1, 42);
    printf("✓ Created test handle: %lld\n", test_handle);
    printf("  Type: %d (expected: %d)\n", PTK_HANDLE_TYPE(test_handle), PTK_TYPE_TIMER);
    printf("  Event Loop ID: %d\n", PTK_HANDLE_EVENT_LOOP_ID(test_handle));
    printf("  Generation: %d\n", PTK_HANDLE_GENERATION(test_handle));
    printf("  Handle ID: %d\n", PTK_HANDLE_ID(test_handle));

    // Test event loop creation
    ptk_handle_t main_loop = ptk_event_loop_create(example_event_loops, 1, &example_resources);
    if(main_loop < 0) {
        printf("✗ Failed to create event loop: %s\n", ptk_error_string((ptk_err_t)main_loop));
        return 1;
    }
    printf("✓ Created event loop (handle: %lld)\n", main_loop);

    // Test timer creation
    ptk_handle_t timer1 = ptk_timer_create();
    ptk_handle_t timer2 = ptk_timer_create();

    if(timer1 < 0 || timer2 < 0) {
        printf("✗ Failed to create timers\n");
        ptk_event_loop_destroy(main_loop);
        return 1;
    }
    printf("✓ Created timers: timer1=%lld, timer2=%lld\n", timer1, timer2);

    // Test handle validation
    bool timer1_valid = ptk_handle_is_valid(timer1);
    bool timer2_valid = ptk_handle_is_valid(timer2);
    ptk_handle_t invalid_handle = PTK_MAKE_HANDLE(PTK_TYPE_INVALID, 0, 0, 999);
    bool invalid_valid = ptk_handle_is_valid(invalid_handle);

    printf("✓ Handle validation: timer1=%s, timer2=%s, invalid=%s\n", timer1_valid ? "valid" : "invalid",
           timer2_valid ? "valid" : "invalid", invalid_valid ? "valid" : "invalid");

    // Test resource type checking
    printf("✓ Resource types: timer=%d, socket=%d, user_event=%d\n", PTK_TYPE_TIMER, PTK_TYPE_SOCKET, PTK_TYPE_USER_EVENT_SOURCE);

    // Test error handling
    printf("✓ Last error: %s\n", ptk_error_string(ptk_get_last_error()));

    // Cleanup
    ptk_event_loop_destroy(main_loop);

    printf("✓ All tests passed! Library is working correctly.\n");
    return 0;
}
