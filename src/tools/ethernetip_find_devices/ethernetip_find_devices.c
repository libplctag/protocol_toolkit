#include "ethernetip_defs.h"
#include "ptk_loop.h"
#include <arpa/inet.h>
#include <signal.h>
#include <string.h>

//=============================================================================
// GLOBAL STATE
//=============================================================================

static ptk_loop *g_loop = NULL;
static bool g_shutdown = false;
static int g_responses_received = 0;
static ptk_network_info *g_networks = NULL;
static size_t g_num_networks = 0;

//=============================================================================
// SIGNAL HANDLING
//=============================================================================

static void signal_handler(int sig) {
    info("Received signal %d, shutting down...", sig);
    g_shutdown = true;
    if(g_loop) { ptk_loop_stop(g_loop); }
}

//=============================================================================
// TIMER CALLBACK
//=============================================================================

/**
 * Timer callback to send periodic List Identity broadcasts
 */
static void broadcast_timer_handler(const ptk_event *event) {
    trace("Broadcast timer handler called, event type: %s", ptk_event_string(event->type));

    if(event->type != PTK_EVENT_TICK) { return; }

    if(g_shutdown) { return; }

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

    // Send broadcast to all discovered networks (using the UDP socket from user_data)
    ptk_sock *udp_sock = (ptk_sock *)event->user_data;
    if(udp_sock) {
        if(g_networks && g_num_networks > 0) {
            // Send to all discovered broadcast addresses
            for(size_t i = 0; i < g_num_networks; i++) {
                info("About to call ptk_udp_send to %s:44818", g_networks[i].broadcast);

                // Need to create a new buffer for each send
                buf *send_buf;
                buf_err_t buf_result = buf_alloc(&send_buf, 64);
                if(buf_result != BUF_OK) {
                    error("Failed to allocate send buffer for network %zu", i);
                    continue;
                }

                // Re-encode the request for this send
                buf_result = eip_list_identity_request_encode(send_buf, request);
                if(buf_result != BUF_OK) {
                    error("Failed to encode request for network %zu", i);
                    buf_free(send_buf);
                    continue;
                }

                ptk_err send_result = ptk_udp_send(udp_sock, &send_buf, g_networks[i].broadcast, 44818);
                info("ptk_udp_send to %s returned: %s", g_networks[i].broadcast, ptk_err_string(send_result));
                if(send_result != PTK_OK) {
                    error("Failed to send broadcast to %s: %s", g_networks[i].broadcast, ptk_err_string(send_result));
                } else {
                    info("Successfully sent List Identity broadcast to %s:44818", g_networks[i].broadcast);
                }
            }

            // Free the original buffer
            buf_free(request_buf);
        } else {
            // Fallback: send to default broadcast if no networks discovered
            warn("No networks discovered, using fallback broadcast to 255.255.255.255:44818");
            ptk_err send_result = ptk_udp_send(udp_sock, &request_buf, "255.255.255.255", 44818);
            info("ptk_udp_send returned: %s", ptk_err_string(send_result));
            if(send_result != PTK_OK) {
                error("Failed to send fallback broadcast: %s", ptk_err_string(send_result));
                buf_free(request_buf);
            } else {
                info("Successfully sent fallback List Identity broadcast to 255.255.255.255:44818");
            }
        }
    } else {
        error("No UDP socket available for broadcast");
        buf_free(request_buf);
    }

    // Cleanup
    eip_list_identity_request_dispose(request);
}

//=============================================================================
// EVENT HANDLERS
//=============================================================================

/**
 * Handle UDP responses from EtherNet/IP devices
 */
static void udp_response_handler(const ptk_event *event) {
    info("UDP response handler called, event type: %s", ptk_event_string(event->type));

    switch(event->type) {
        case PTK_EVENT_READ: {
            if(!event->data || !*event->data) {
                warn("Received UDP read event with no data");
                return;
            }

            buf *response_buf = *event->data;
            info("Received UDP response from %s:%d, %zu bytes", event->remote_host, event->remote_port, response_buf->len);

            // Try to decode as List Identity Response
            eip_list_identity_response *response;
            buf_err_t result = eip_list_identity_response_decode(&response, response_buf);
            if(result != BUF_OK) {
                warn("Failed to decode List Identity Response: %d", result);
                buf_free(response_buf);
                return;
            }

            g_responses_received++;

            info("=== EtherNet/IP Device Discovery Response #%d ===", g_responses_received);
            info("From: %s:%d", event->remote_host, event->remote_port);
            eip_list_identity_response_log_info(response);

            // Parse and display device information
            for(int i = 0; i < response->item_count; i++) {
                if(!response->items[i]) { continue; }

                switch(response->items[i]->type_id) {
                    case CPF_TYPE_ID_CIP_IDENTITY: {
                        eip_cpf_cip_identity_item *identity = (eip_cpf_cip_identity_item *)response->items[i];
                        info("Device Identity:");
                        info("  Vendor ID: 0x%04X", identity->vendor_id);
                        info("  Device Type: 0x%04X", identity->device_type);
                        info("  Product Code: 0x%04X", identity->product_code);
                        info("  Revision: %u.%u", identity->major_revision, identity->minor_revision);
                        info("  Status: 0x%04X", identity->status);
                        info("  Serial Number: 0x%08X (%u)", identity->serial_number, identity->serial_number);
                        if(identity->product_name && identity->product_name_length > 0) {
                            info("  Product Name: %s", (char *)identity->product_name);
                        }
                        break;
                    }

                    case CPF_TYPE_ID_SOCKET_ADDR: {
                        eip_cpf_socket_addr_item *socket_addr = (eip_cpf_socket_addr_item *)response->items[i];

                        // Convert network byte order to host byte order for display
                        uint16_t family = ntohs(socket_addr->sin_family);
                        uint16_t port = ntohs(socket_addr->sin_port);
                        uint32_t addr = socket_addr->sin_addr;  // Already in network byte order

                        uint8_t *addr_bytes = (uint8_t *)&addr;
                        info("Socket Address:");
                        info("  Family: %u", family);
                        info("  Port: %u", port);
                        info("  Address: %u.%u.%u.%u", addr_bytes[0], addr_bytes[1], addr_bytes[2], addr_bytes[3]);
                        break;
                    }

                    default:
                        info("CPF Item Type: 0x%04X (length: %u)", response->items[i]->type_id, response->items[i]->length);
                        break;
                }
            }
            info("================================================");

            // Cleanup
            eip_list_identity_response_dispose(response);
            buf_free(response_buf);
            break;
        }

        case PTK_EVENT_WRITE_DONE: trace("UDP send completed"); break;

        case PTK_EVENT_ERROR: error("UDP socket error: %s", ptk_err_string(event->error)); break;

        case PTK_EVENT_CLOSE: info("UDP socket closed"); break;

        default: trace("Unhandled UDP event: %s", ptk_event_string(event->type)); break;
    }
}

//=============================================================================
// MAIN FUNCTION
//=============================================================================

int main(int argc, char *argv[]) {
    info("Starting EtherNet/IP Client (Device Discovery)");

    // Parse command line arguments
    int broadcast_interval = 5;  // Default 5 seconds
    int discovery_time = 30;     // Default 30 seconds

    if(argc > 1) {
        broadcast_interval = atoi(argv[1]);
        if(broadcast_interval < 1) { broadcast_interval = 5; }
    }

    if(argc > 2) {
        discovery_time = atoi(argv[2]);
        if(discovery_time < 1) { discovery_time = 30; }
    }

    info("Configuration:");
    info("  Broadcast interval: %d seconds", broadcast_interval);
    info("  Discovery time: %d seconds", discovery_time);

    // Set up signal handling
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Discover network interfaces
    info("Discovering network interfaces...");
    ptk_err net_result = ptk_loop_find_networks(&g_networks, &g_num_networks, NULL);
    if(net_result != PTK_OK) {
        warn("Failed to discover networks: %s", ptk_err_string(net_result));
        info("Will use fallback broadcast addressing");
    } else {
        info("Discovered %zu network interfaces:", g_num_networks);
        for(size_t i = 0; i < g_num_networks; i++) {
            info("  Interface %zu: IP=%s, Netmask=%s, Broadcast=%s", i + 1, g_networks[i].network_ip, g_networks[i].netmask,
                 g_networks[i].broadcast);
        }
    }

    // Create event loop
    ptk_loop_opts loop_opts = {.worker_threads = 1, .max_events = 32, .auto_start = true};

    ptk_err result = ptk_loop_create(&g_loop, &loop_opts);
    if(result != PTK_OK) {
        error("Failed to create event loop: %s", ptk_err_string(result));
        return 1;
    }

    // Create UDP socket for requests and responses (bind to local interface)
    ptk_udp_opts udp_opts = {.bind_host = "0.0.0.0",  // Bind to our local IP
                             .bind_port = 0,          // Let system choose port
                             .callback = udp_response_handler,
                             .user_data = NULL,
                             .broadcast = true,  // Back to broadcast mode
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
        info("UDP socket created and bound to %s:%d for EtherNet/IP discovery", local_host, local_port);
    } else {
        info("UDP socket created for EtherNet/IP discovery");
    }

    // Create timer for periodic broadcasts
    ptk_timer_opts broadcast_timer_opts = {
        .timeout_ms = broadcast_interval * 1000,
        .repeat = true,
        .callback = broadcast_timer_handler,
        .user_data = udp_sock  // Pass UDP socket to timer
    };

    ptk_sock *broadcast_timer;
    result = ptk_timer_start(g_loop, &broadcast_timer, &broadcast_timer_opts);
    if(result != PTK_OK) {
        error("Failed to start broadcast timer: %s", ptk_err_string(result));
        ptk_close(udp_sock);
        ptk_loop_destroy(g_loop);
        return 1;
    }

    // Create timer to stop discovery after specified time
    ptk_timer_opts stop_timer_opts = {.timeout_ms = discovery_time * 1000,
                                      .repeat = false,
                                      .callback = NULL,  // We'll check in main loop
                                      .user_data = NULL};

    ptk_sock *stop_timer;
    result = ptk_timer_start(g_loop, &stop_timer, &stop_timer_opts);
    if(result != PTK_OK) {
        error("Failed to start stop timer: %s", ptk_err_string(result));
        ptk_timer_stop(broadcast_timer);
        ptk_close(udp_sock);
        ptk_loop_destroy(g_loop);
        return 1;
    }

    info("EtherNet/IP Device Discovery started");
    info("Broadcasting List Identity requests every %d seconds...", broadcast_interval);
    info("Discovery will run for %d seconds", discovery_time);
    info("Press Ctrl+C to stop early...");

    // Trigger immediate broadcast
    ptk_event immediate_event = {.type = PTK_EVENT_TICK, .sock = broadcast_timer, .user_data = udp_sock};
    broadcast_timer_handler(&immediate_event);

    // Wait for discovery time or manual stop
    result = ptk_loop_wait_timeout(g_loop, discovery_time * 1000);
    if(result == PTK_ERR_TIMEOUT) {
        info("Discovery time completed");
        g_shutdown = true;
        ptk_loop_stop(g_loop);
    } else if(result != PTK_OK) {
        error("Event loop error: %s", ptk_err_string(result));
    }

    // Cleanup
    info("Shutting down EtherNet/IP Client");
    ptk_timer_stop(broadcast_timer);
    ptk_timer_stop(stop_timer);
    ptk_close(udp_sock);
    ptk_loop_destroy(g_loop);

    // Clean up network discovery data
    if(g_networks) {
        ptk_network_info_dispose(g_networks, g_num_networks);
        g_networks = NULL;
        g_num_networks = 0;
    }

    info("=== EtherNet/IP Device Discovery Summary ===");
    info("Total devices discovered: %d", g_responses_received);
    info("Discovery completed");

    return 0;
}
