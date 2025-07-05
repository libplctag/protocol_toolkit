#include "src/protocol_toolkit/ev_loop/ev_loop.h"
#include "src/protocol_toolkit/utils/log.h"
#include <stdio.h>
#include <signal.h>
#include <stdbool.h>

static ev_loop *g_loop = NULL;
static bool g_shutdown = false;
static int g_timer_count = 0;

static void signal_handler(int sig) {
    info("Received signal %d, shutting down...", sig);
    g_shutdown = true;
    if(g_loop) { 
        ev_loop_stop(g_loop); 
    }
}

static void timer_callback(const ev_event *event) {
    info("Timer fired! Count: %d, Event type: %s", ++g_timer_count, ev_event_string(event->type));
    
    if (g_timer_count >= 5) {
        info("Timer fired 5 times, stopping...");
        g_shutdown = true;
        ev_loop_stop(g_loop);
    }
}

int main() {
    info("Testing timer functionality...");
    
    // Set up signal handling
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Create event loop
    ev_loop_opts loop_opts = {
        .worker_threads = 1, 
        .max_events = 32, 
        .auto_start = true
    };
    
    ev_err result = ev_loop_create(&g_loop, &loop_opts);
    if(result != EV_OK) {
        error("Failed to create event loop: %s", ev_err_string(result));
        return 1;
    }
    
    info("Event loop created successfully");
    
    // Create a timer that fires every 1 second
    ev_timer_opts timer_opts = {
        .timeout_ms = 1000,
        .repeat = true,
        .callback = timer_callback,
        .user_data = NULL
    };
    
    ev_sock *timer;
    result = ev_timer_start(g_loop, &timer, &timer_opts);
    if(result != EV_OK) {
        error("Failed to start timer: %s", ev_err_string(result));
        ev_loop_destroy(g_loop);
        return 1;
    }
    
    info("Timer started, waiting for events...");
    
    // Wait for 10 seconds or until shutdown
    result = ev_loop_wait_timeout(g_loop, 10000);
    if(result == EV_ERR_TIMEOUT) {
        info("Test timeout reached");
    } else if(result != EV_OK) {
        error("Event loop error: %s", ev_err_string(result));
    }
    
    // Cleanup
    info("Cleaning up...");
    ev_timer_stop(timer);
    ev_loop_destroy(g_loop);
    
    info("Timer test completed. Timer fired %d times.", g_timer_count);
    return g_timer_count > 0 ? 0 : 1;
}