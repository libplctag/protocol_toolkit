#include "src/protocol_toolkit/ev_loop/ev_loop.h"
#include "src/protocol_toolkit/utils/log.h"
#include "src/protocol_toolkit/utils/buf.h"
#include <stdio.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>

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

static void udp_handler(const ev_event *event) {
    info("UDP event: %s", ev_event_string(event->type));
    
    switch(event->type) {
        case EV_EVENT_READ:
            if(event->data && *event->data) {
                buf *response_buf = *event->data;
                info("Received UDP data from %s:%d, %zu bytes", 
                     event->remote_host, event->remote_port, response_buf->len);
                buf_free(response_buf);
            }
            break;
        case EV_EVENT_WRITE_DONE:
            info("UDP send completed");
            break;
        case EV_EVENT_ERROR:
            error("UDP socket error: %s", ev_err_string(event->error));
            break;
        case EV_EVENT_CLOSE:
            info("UDP socket closed");
            break;
        default:
            info("Unhandled UDP event: %s", ev_event_string(event->type));
            break;
    }
}

static void timer_callback(const ev_event *event) {
    info("Timer fired! Count: %d", ++g_timer_count);
    
    // Send a test UDP packet
    ev_sock *udp_sock = (ev_sock *)event->user_data;
    if(udp_sock) {
        buf *test_buf;
        buf_err_t result = buf_alloc(&test_buf, 64);
        if(result == BUF_OK) {
            const char *test_data = "Hello UDP!";
            memcpy(test_buf->data, test_data, strlen(test_data));
            test_buf->cursor = strlen(test_data);
            
            ev_err send_result = ev_udp_send(udp_sock, &test_buf, "127.0.0.1", 12345);
            if(send_result != EV_OK) {
                error("Failed to send UDP packet: %s", ev_err_string(send_result));
            } else {
                info("Sent test UDP packet");
            }
        }
    }
    
    if (g_timer_count >= 3) {
        info("Timer fired 3 times, stopping...");
        g_shutdown = true;
        ev_loop_stop(g_loop);
    }
}

int main() {
    info("Testing UDP + Timer functionality...");
    
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
    
    // Create UDP socket
    ev_udp_opts udp_opts = {
        .bind_host = "0.0.0.0",
        .bind_port = 0,  // Let system choose port
        .callback = udp_handler,
        .user_data = NULL,
        .broadcast = false,
        .reuse_addr = true,
        .read_buffer_size = 1024
    };
    
    ev_sock *udp_sock;
    result = ev_udp_create(g_loop, &udp_sock, &udp_opts);
    if(result != EV_OK) {
        error("Failed to create UDP socket: %s", ev_err_string(result));
        ev_loop_destroy(g_loop);
        return 1;
    }
    
    info("UDP socket created");
    
    // Create a timer that fires every 2 seconds
    ev_timer_opts timer_opts = {
        .timeout_ms = 2000,
        .repeat = true,
        .callback = timer_callback,
        .user_data = udp_sock  // Pass UDP socket to timer
    };
    
    ev_sock *timer;
    result = ev_timer_start(g_loop, &timer, &timer_opts);
    if(result != EV_OK) {
        error("Failed to start timer: %s", ev_err_string(result));
        ev_close(udp_sock);
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
    ev_close(udp_sock);
    ev_loop_destroy(g_loop);
    
    info("UDP+Timer test completed. Timer fired %d times.", g_timer_count);
    return g_timer_count > 0 ? 0 : 1;
}