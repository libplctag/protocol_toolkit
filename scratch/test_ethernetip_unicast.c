#include "src/protocol_toolkit/ptk_loop/ptk_loop.h"
#include "src/protocol_toolkit/utils/log.h"
#include "src/protocols/ethernetip/protocol/ethernetip_defs.h"
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static ptk_loop *g_loop = NULL;
static bool g_shutdown = false;
static int g_responses_received = 0;

static void signal_handler(int sig) {
    info("Received signal %d, shutting down...", sig);
    g_shutdown = true;
    if(g_loop) { ptk_loop_stop(g_loop); }
}

static void udp_response_handler(const ptk_event *event) {
    info("UDP event: %s", ptk_event_string(event->type));

    switch(event->type) {
        case PTK_EVENT_READ: {
            if(!event->data || !*event->data) {
                warn("Received UDP read event with no data");
                return;
            }

            buf *response_buf = *event->data;
            info("Received UDP response from %s:%d, %zu bytes", event->remote_host, event->remote_port, response_buf->len);

            g_responses_received++;

            // Try to decode as List Identity Response
            eip_list_identity_response *response;
            buf_err_t result = eip_list_identity_response_decode(&response, response_buf);
            if(result != BUF_OK) {
                warn("Failed to decode List Identity Response: %d", result);
                info("Raw response data (first 32 bytes):");
                for(size_t i = 0; i < response_buf->len && i < 32; i++) {
                    printf("%02X ", response_buf->data[i]);
                    if((i + 1) % 16 == 0) { printf("\n"); }
                }
                printf("\n");
                buf_free(response_buf);
                return;
            }

            info("=== EtherNet/IP Device Discovery Response #%d ===", g_responses_received);
            info("From: %s:%d", event->remote_host, event->remote_port);
            eip_list_identity_response_log_info(response);

            // Cleanup
            eip_list_identity_response_dispose(response);
            buf_free(response_buf);
            break;
        }

        case PTK_EVENT_WRITE_DONE: info("UDP send completed"); break;

        case PTK_EVENT_ERROR: error("UDP socket error: %s", ptk_err_string(event->error)); break;

        case PTK_EVENT_CLOSE: info("UDP socket closed"); break;

        default: info("Unhandled UDP event: %s", ptk_event_string(event->type)); break;
    }
}

static void send_list_identity_to_host(ptk_sock *udp_sock, const char *host) {
    info("Sending List Identity request to %s:2222", host);

    // Create List Identity Request
    eip_list_identity_request *request = calloc(1, sizeof(eip_list_identity_request));
    if(!request) {
        error("Failed to allocate List Identity Request");
        return;
    }

    // Allocate buffer for request
    buf *request_buf;
    buf_err_t result = buf_alloc(&request_buf, 64);
    if(result != BUF_OK) {
        error("Failed to allocate request buffer");
        free(request);
        return;
    }

    // Encode the request
    result = eip_list_identity_request_encode(request_buf, request);
    if(result != BUF_OK) {
        error("Failed to encode List Identity Request");
        buf_free(request_buf);
        eip_list_identity_request_dispose(request);
        return;
    }

    eip_list_identity_request_log_info(request);

    // Send to specific host
    info("Attempting to send %zu byte packet to %s:2222", request_buf->cursor, host);
    ptk_err send_result = ptk_udp_send(udp_sock, &request_buf, host, 2222);
    if(send_result != PTK_OK) {
        error("Failed to send request to %s: %s (error code: %d)", host, ptk_err_string(send_result), send_result);
    } else {
        info("Successfully sent List Identity request to %s:2222", host);
    }

    // Cleanup
    eip_list_identity_request_dispose(request);
}

int main() {
    info("Testing EtherNet/IP unicast to specific PLCs...");

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

    // Create UDP socket (unbound for sending only)
    ptk_udp_opts udp_opts = {.bind_host = NULL,  // Don't bind at all
                             .bind_port = 0,     // Let system choose port for unicast test
                             .callback = udp_response_handler,
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

    // Get and display the socket's local address
    char local_host[64];
    int local_port;
    if(ptk_sock_get_local_addr(udp_sock, local_host, sizeof(local_host), &local_port) == PTK_OK) {
        info("UDP socket bound to %s:%d", local_host, local_port);
    }

    // Send requests to specific PLCs
    send_list_identity_to_host(udp_sock, "10.206.1.39");
    send_list_identity_to_host(udp_sock, "10.206.1.40");

    info("Waiting 10 seconds for responses...");

    // Wait for responses
    result = ptk_loop_wait_timeout(g_loop, 10000);
    if(result == PTK_ERR_TIMEOUT) {
        info("Test timeout reached");
    } else if(result != PTK_OK) {
        error("Event loop error: %s", ptk_err_string(result));
    }

    // Cleanup
    info("Cleaning up...");
    ptk_close(udp_sock);
    ptk_loop_destroy(g_loop);

    info("Unicast test completed. Received %d responses.", g_responses_received);
    return g_responses_received > 0 ? 0 : 1;
}