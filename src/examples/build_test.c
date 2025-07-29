/**
 * @file build_test.c
 * @brief Simple build test for the Protocol Toolkit macOS implementation
 */

#include "../../include/macos/protocol_toolkit.h"
#include <stdio.h>

/* Declare event loop slots and resources */
PTK_DECLARE_EVENT_LOOP_SLOTS(test_event_loops, 1);
PTK_DECLARE_EVENT_LOOP_RESOURCES(test_resources, 1, 0, 0);

int main(void) {
    printf("Protocol Toolkit macOS Build Test\n");
    printf("==================================\n");

    /* Test basic handle manipulation */
    ptk_handle_t test_handle = PTK_MAKE_HANDLE(PTK_TYPE_TIMER, 0, 1, 42);
    printf("Created test handle: %lld\n", test_handle);
    printf("  Type: %d\n", PTK_HANDLE_TYPE(test_handle));
    printf("  Event Loop ID: %d\n", PTK_HANDLE_EVENT_LOOP_ID(test_handle));
    printf("  Generation: %d\n", PTK_HANDLE_GENERATION(test_handle));
    printf("  Handle ID: %d\n", PTK_HANDLE_ID(test_handle));

    /* Test event loop creation */
    ptk_handle_t main_loop = ptk_event_loop_create(test_event_loops, 1, &test_resources);
    if(main_loop < 0) {
        printf("Error: Failed to create event loop: %s\n", ptk_error_string((ptk_err_t)main_loop));
        return 1;
    }
    printf("Created event loop with handle: %lld\n", main_loop);

    /* Test timer creation */
    ptk_handle_t timer = ptk_timer_create(main_loop);
    if(timer < 0) {
        printf("Error: Failed to create timer: %s\n", ptk_error_string((ptk_err_t)timer));
        ptk_event_loop_destroy(main_loop);
        return 1;
    }
    printf("Created timer with handle: %lld\n", timer);

    /* Test handle validation */
    bool valid = ptk_handle_is_valid(timer);
    printf("Timer handle is valid: %s\n", valid ? "true" : "false");

    /* Test resource type */
    ptk_resource_type_t type = ptk_handle_get_type(timer);
    printf("Timer handle type: %d (expected: %d)\n", type, PTK_TYPE_TIMER);

    /* Clean up */
    ptk_timer_destroy(timer);
    ptk_event_loop_destroy(main_loop);

    printf("Build test completed successfully!\n");
    return 0;
}
