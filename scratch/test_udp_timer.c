#include "src/protocol_toolkit/ptk_loop/ptk_loop.h"
#include "src/protocol_toolkit/utils/buf.h"
#include "src/protocol_toolkit/utils/log.h"
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static ptk_loop *g_loop = NULL;
static bool g_shutdown = false;
static int g_timer_count = 0;

static void signal_handler(int sig) {
    info("Received signal %d, shutting down...", sig);
    g_shutdown = true;
    if(g_loop) { ptk_loop_stop(g_loop); }
}

static void udp_handler(const ptk_event *event) {
    info("UDP event: %s", ptk_event_string(event->type));

    switch(event->type) {
        case PTK_EVENT_READ:
            if(event->data && *event->data) {
                buf *response_buf = *event->data;
                info("Received UDP data from %s:%d, %zu bytes", event->remote_host, event->remote_port, response_buf->len);
                buf_free(response_buf);
            }
            break;
        case PTK_EVENT_WRITE_DONE: info("UDP send completed"); break;
        case PTK_EVENT_ERROR: error("UDP socket error: %s", ptk_err_string(event->error)); break;
        case PTK_EVENT_CLOSE: info("UDP socket closed"); break;
        default: info("Unhandled UDP event: %s", ptk_event_string(event->type)); break;
    }
}

static void timer_callback(const ptk_event *event) {
    info("Timer fired! Count: %d", ++g_timer_count);

    // Send a test UDP packet
    ptk_sock *udp_sock = (ptk_sock *)event->user_data;
    if(udp_sock) {
        buf *test_buf;
        buf_err_t result = buf_alloc(&test_buf, 64);
        if(result == BUF_OK) {
            const char *test_data = "Hello UDP!";
            memcpy(test_buf->data, test_data, strlen(test_data));
            test_buf->cursor = strlen(test_data);

            ptk_err send_result = ptk_udp_send(udp_sock, &test_buf, "127.0.0.1", 12345);
            if(send_result != PTK_OK) {
                error("Failed to send UDP packet: %s", ptk_err_string(send_result));
            } else {
                info("Sent test UDP packet");
            }
        }
    }

    if(g_timer_count >= 3) {
        info("Timer fired 3 times, stopping...");
        g_shutdown = true;
        ptk_loop_stop(g_loop);
    }
}

int main() {
    info("Testing UDP + Timer functionality...");

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

    // Create UDP socket
    ptk_udp_opts udp_opts = {.bind_host = "0.0.0.0",
                             .bind_port = 0,  // Let system choose port
                             .callback = udp_handler,
                             .user_data = NULL,
                             .broadcast = false,
                             .reuse_addr = true,
                             .read_buffer_size = 1024};

    ptk_sock *udp_sock;
    result = ptk_udp_create(g_loop, &udp_sock, &udp_opts);
    if(result != PTK_OK) {
        error("Failed to create UDP socket: %s", ptk_err_string(result));
        ptk_loop_destroy(g_loop);
        return 1;
    }

    info("UDP socket created");

    // Create a timer that fires every 2 seconds
    ptk_timer_opts timer_opts = {
        .timeout_ms = 2000,
        .repeat = true,
        .callback = timer_callback,
        .user_data = udp_sock  // Pass UDP socket to timer
    };

    ptk_sock *timer;
    result = ptk_timer_start(g_loop, &timer, &timer_opts);
    if(result != PTK_OK) {
        error("Failed to start timer: %s", ptk_err_string(result));
        ptk_close(udp_sock);
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
    ptk_close(udp_sock);
    ptk_loop_destroy(g_loop);

    info("UDP+Timer test completed. Timer fired %d times.", g_timer_count);
    return g_timer_count > 0 ? 0 : 1;
}