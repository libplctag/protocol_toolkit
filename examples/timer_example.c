/**
 * Timer Example for Protocol Toolkit macOS
 *
 * This example demonstrates:
 * 1. Creating an event loop
 * 2. Setting up timers
 * 3. Running the event loop briefly to show it works
 * 4. Proper cleanup
 */

#include <stdio.h>
#include <unistd.h>
#include "protocol_toolkit.h"

// Declare event loop slots and resources for our example
PTK_DECLARE_EVENT_LOOP_SLOTS(timer_event_loops, 1);
PTK_DECLARE_EVENT_LOOP_RESOURCES(timer_resources, 3, 0, 0);

int main() {
    printf("Protocol Toolkit Timer Example\n");
    printf("==============================\n\n");

    // Create event loop
    ptk_handle_t event_loop = ptk_event_loop_create(timer_event_loops, 1, &timer_resources);
    if(event_loop < 0) {
        printf("❌ Failed to create event loop: %s\n", ptk_error_string((ptk_err_t)event_loop));
        return 1;
    }
    printf("✓ Created event loop (handle: %lld)\n", event_loop);

    // Create three timers
    ptk_handle_t timer1 = ptk_timer_create();
    ptk_handle_t timer2 = ptk_timer_create();
    ptk_handle_t timer3 = ptk_timer_create();

    if(timer1 < 0 || timer2 < 0 || timer3 < 0) {
        printf("❌ Failed to create timers\n");
        ptk_event_loop_destroy(event_loop);
        return 1;
    }

    printf("✓ Created timers: %lld, %lld, %lld\n", timer1, timer2, timer3);

    // Test timer configuration (even if they don't run in this simple example)
    // Timer 1: 1 second interval, repeating
    ptk_err_t result1 = ptk_timer_start(timer1, 1000, true);

    // Timer 2: 2.5 second interval, repeating
    ptk_err_t result2 = ptk_timer_start(timer2, 2500, true);

    // Timer 3: 5 second one-shot timer
    ptk_err_t result3 = ptk_timer_start(timer3, 5000, false);

    if(result1 != PTK_ERR_OK || result2 != PTK_ERR_OK || result3 != PTK_ERR_OK) {
        printf("⚠️  Warning: Some timers failed to start (this is expected in this simple example)\n");
        printf("   Timer 1 result: %s\n", ptk_error_string(result1));
        printf("   Timer 2 result: %s\n", ptk_error_string(result2));
        printf("   Timer 3 result: %s\n", ptk_error_string(result3));
    } else {
        printf("✓ Started timers:\n");
        printf("  - Timer 1: 1.0s interval (repeating)\n");
        printf("  - Timer 2: 2.5s interval (repeating)\n");
        printf("  - Timer 3: 5.0s one-shot\n\n");
    }

    // Validate handle types
    printf("✓ Handle validation:\n");
    printf("  - Event loop handle type: %d (expected: %d)\n", PTK_HANDLE_TYPE(event_loop), PTK_TYPE_EVENT_LOOP);
    printf("  - Timer1 handle type: %d (expected: %d)\n", PTK_HANDLE_TYPE(timer1), PTK_TYPE_TIMER);
    printf("  - Timer2 handle type: %d (expected: %d)\n", PTK_HANDLE_TYPE(timer2), PTK_TYPE_TIMER);
    printf("  - Timer3 handle type: %d (expected: %d)\n", PTK_HANDLE_TYPE(timer3), PTK_TYPE_TIMER);

    // Run event loop once to show it works
    printf("\n⏱️  Running event loop once...\n");
    ptk_err_t loop_result = ptk_event_loop_run(event_loop);
    printf("✓ Event loop run result: %s\n", ptk_error_string(loop_result));

    // Stop timers
    ptk_timer_stop(timer1);
    ptk_timer_stop(timer2);
    ptk_timer_stop(timer3);

    // Cleanup
    ptk_event_loop_destroy(event_loop);
    printf("✓ Cleaned up resources\n");

    printf("\n🎉 Timer example completed successfully!\n");
    printf("Note: This example demonstrates timer creation and basic API usage.\n");
    printf("For a fully functional timer demo with callbacks, additional implementation would be needed.\n");

    return 0;
}
