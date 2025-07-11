/**
 * @file timer_example.c
 * @brief Example usage of the minimal timer API
 *
 * This demonstrates how the simple timer API integrates with protocol implementations.
 * Shows both one-shot and repeating timers in a realistic protocol context.
 */

#include "ptk_timer.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Example: Protocol timeout handler
static bool protocol_timeout_handler(void *user_data) {
    int *request_id = (int *)user_data;
    printf("Protocol request %d timed out!\n", *request_id);

    // Handle timeout - could retry, fail, etc.
    // Return false to stop timer (one-shot behavior)
    return false;
}

// Example: Keepalive handler
static bool keepalive_handler(void *user_data) {
    printf("Sending keepalive...\n");

    // Send keepalive packet here

    // Return true to continue repeating
    return true;
}

// Example: Periodic stats handler
static bool stats_handler(void *user_data) {
    static int stats_count = 0;
    printf("Stats update #%d\n", ++stats_count);

    // Continue for 10 updates, then stop
    return stats_count < 10;
}

int main(void) {
    // Initialize timer system
    if(ptk_timer_init() != 0) {
        fprintf(stderr, "Failed to initialize timer system\n");
        return 1;
    }

    printf("Timer system initialized\n");

    // Example 1: Protocol request timeout (one-shot)
    int request_id = 12345;
    ptk_timer_t timeout_timer = ptk_timer_create_oneshot(5000,  // 5 second timeout
                                                         protocol_timeout_handler, &request_id);

    if(timeout_timer) { printf("Created protocol timeout timer (5s)\n"); }

    // Example 2: Keepalive timer (repeating)
    ptk_timer_t keepalive_timer = ptk_timer_create_repeating(30000,  // 30 second interval
                                                             keepalive_handler, NULL);

    if(keepalive_timer) { printf("Created keepalive timer (30s interval)\n"); }

    // Example 3: Stats timer (repeating with auto-stop)
    ptk_timer_t stats_timer = ptk_timer_create_repeating(2000,  // 2 second interval
                                                         stats_handler, NULL);

    if(stats_timer) { printf("Created stats timer (2s interval, auto-stop after 10)\n"); }

    // Simple event loop simulation
    printf("\nStarting event loop...\n");

    for(int i = 0; i < 60; i++) {  // Run for 60 seconds
        // Get next timeout for event loop
        int timeout = ptk_timer_get_next_timeout();

        if(timeout == -1) {
            printf("No active timers, exiting\n");
            break;
        }

        printf("Next timer in %dms\n", timeout);

        // Simulate waiting (in real code, this would be select/poll)
        usleep(timeout * 1000);

        // Process expired timers
        int processed = ptk_timer_process_expired();
        if(processed > 0) { printf("Processed %d expired timers\n", processed); }

        // Cancel timeout timer after 3 seconds (simulate successful response)
        if(i == 3 && ptk_timer_is_active(timeout_timer)) {
            printf("Canceling timeout timer (response received)\n");
            ptk_timer_cancel(timeout_timer);
        }
    }

    // Cleanup
    ptk_timer_cleanup();
    printf("Timer system cleaned up\n");

    return 0;
}
