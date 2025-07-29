/**
 * @file simple_timer_example.c
 * @brief Simple example showing Protocol Toolkit timer usage on macOS
 */

#include "protocol_toolkit.h"
#include <stdio.h>
#include <unistd.h>

/* Declare event loop slots and resources */
PTK_DECLARE_EVENT_LOOP_SLOTS(app_event_loops, 1);
PTK_DECLARE_EVENT_LOOP_RESOURCES(main_resources, 2, 0, 0);

static int timer_count = 0;

void timer_handler(ptk_handle_t resource, ptk_event_type_t event_type, void *event_data, void *user_data) {
    (void)event_data; /* unused */

    printf("Timer expired! Count: %d (resource handle: %lld, user_data: %s)\n", ++timer_count, resource, (char *)user_data);

    if(timer_count >= 5) {
        printf("Stopping after 5 timer events\n");
        ptk_timer_stop(resource);
    }
}

int main(void) {
    printf("Protocol Toolkit macOS Timer Example\n");
    printf("=====================================\n");

    /* Create event loop */
    ptk_handle_t main_loop = ptk_event_loop_create(app_event_loops, 1, &main_resources);
    if(main_loop < 0) {
        printf("Error: Failed to create event loop: %s\n", ptk_error_string((ptk_err_t)main_loop));
        return 1;
    }
    printf("Created event loop with handle: %lld\n", main_loop);

    /* Create timer */
    ptk_handle_t timer = ptk_timer_create(main_loop);
    if(timer < 0) {
        printf("Error: Failed to create timer: %s\n", ptk_error_string((ptk_err_t)timer));
        ptk_event_loop_destroy(main_loop);
        return 1;
    }
    printf("Created timer with handle: %lld\n", timer);

    /* Set timer event handler */
    ptk_err_t result = ptk_set_event_handler(timer, PTK_EVENT_TIMER_EXPIRED, timer_handler, "example_data");
    if(result != PTK_ERR_OK) {
        printf("Error: Failed to set timer handler: %s\n", ptk_error_string(result));
        ptk_timer_destroy(timer);
        ptk_event_loop_destroy(main_loop);
        return 1;
    }
    printf("Set timer event handler\n");

    /* Start timer (1 second interval, repeating) */
    result = ptk_timer_start(timer, 1000, true);
    if(result != PTK_ERR_OK) {
        printf("Error: Failed to start timer: %s\n", ptk_error_string(result));
        ptk_timer_destroy(timer);
        ptk_event_loop_destroy(main_loop);
        return 1;
    }
    printf("Started 1-second repeating timer\n");
    printf("Running event loop...\n\n");

    /* Run event loop until timer stops */
    while(timer_count < 5) {
        result = ptk_event_loop_run(main_loop);
        if(result != PTK_ERR_OK) {
            printf("Error: Event loop run failed: %s\n", ptk_error_string(result));
            break;
        }

        /* Small sleep to prevent busy waiting */
        usleep(50000); /* 50ms */
    }

    printf("\nCleaning up...\n");

    /* Clean up */
    ptk_timer_destroy(timer);
    ptk_event_loop_destroy(main_loop);

    printf("Example completed successfully!\n");
    return 0;
}
