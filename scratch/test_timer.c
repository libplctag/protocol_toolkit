#include "src/protocol_toolkit/ptk_loop/ptk_loop.h"
#include "src/protocol_toolkit/utils/log.h"
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>

static ptk_loop *g_loop = NULL;
static bool g_shutdown = false;
static int g_timer_count = 0;

static void signal_handler(int sig) {
    info("Received signal %d, shutting down...", sig);
    g_shutdown = true;
    if(g_loop) { ptk_loop_stop(g_loop); }
}

static void timer_callback(const ptk_event *event) {
    info("Timer fired! Count: %d, Event type: %s", ++g_timer_count, ptk_event_string(event->type));

    if(g_timer_count >= 5) {
        info("Timer fired 5 times, stopping...");
        g_shutdown = true;
        ptk_loop_stop(g_loop);
    }
}

int main() {
    info("Testing timer functionality...");

    // Set up signal handling
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Create event loop
    ptk_loop_opts loop_opts = {.worker_threads = 1, .max_events = 32, .auto_start = true};

    ptk_err result = ptk_loop_create(&g_loop, &loop_opts);
    if(result != PTK_OK) {
        error("Failed to create event loop: %s", ptk_err_string(result));
        return 1;
    }

    info("Event loop created successfully");

    // Create a timer that fires every 1 second
    ptk_timer_opts timer_opts = {.timeout_ms = 1000, .repeat = true, .callback = timer_callback, .user_data = NULL};

    ptk_sock *timer;
    result = ptk_timer_start(g_loop, &timer, &timer_opts);
    if(result != PTK_OK) {
        error("Failed to start timer: %s", ptk_err_string(result));
        ptk_loop_destroy(g_loop);
        return 1;
    }

    info("Timer started, waiting for events...");

    // Wait for 10 seconds or until shutdown
    result = ptk_loop_wait_timeout(g_loop, 10000);
    if(result == PTK_ERR_TIMEOUT) {
        info("Test timeout reached");
    } else if(result != PTK_OK) {
        error("Event loop error: %s", ptk_err_string(result));
    }

    // Cleanup
    info("Cleaning up...");
    ptk_timer_stop(timer);
    ptk_loop_destroy(g_loop);

    info("Timer test completed. Timer fired %d times.", g_timer_count);
    return g_timer_count > 0 ? 0 : 1;
}