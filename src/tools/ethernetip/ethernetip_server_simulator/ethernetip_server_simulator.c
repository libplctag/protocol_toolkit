#include "ev_loop.h"
#include "ethernetip_defs.h"
#include <signal.h>
#include <string.h>
#include <arpa/inet.h>

//=============================================================================
// GLOBAL STATE
//=============================================================================

static ev_loop *g_loop = NULL;
static bool g_shutdown = false;

// Device identity information
#define VENDOR_ID       0x1234      // Example vendor ID
#define DEVICE_TYPE     0x000C      // Communication Device
#define PRODUCT_CODE    0x0001      // Example product code
#define MAJOR_REVISION  1
#define MINOR_REVISION  0
#define DEVICE_STATUS   0x0000      // Status OK
#define SERIAL_NUMBER   0x12345678  // Example serial number
#define PRODUCT_NAME    "EtherNet/IP Test Device"

//=============================================================================
// SIGNAL HANDLING
//=============================================================================

static void signal_handler(int sig) {
    info("Received signal %d, shutting down...", sig);
    g_shutdown = true;
    if (g_loop) {
        ev_loop_stop(g_loop);
    }
}

//=============================================================================
// HELPER FUNCTIONS
//=============================================================================

/**
 * Create a List Identity Response packet
 */
static buf_err_t create_list_identity_response(buf **response_buf, const char *source_ip, int source_port) {
    trace("Creating List Identity Response");
    
    // Allocate response structure
    eip_list_identity_response *response = calloc(1, sizeof(eip_list_identity_response));
    if (!response) {
        error("Failed to allocate List Identity Response");
        return BUF_ERR_NO_RESOURCES;
    }
    
    // Set up response header
    response->header.command = EIP_LIST_IDENTITY;
    response->header.session_handle = 0;
    response->header.status = 0;
    response->header.sender_context = 0;
    response->header.options = 0;
    
    // Create 2 CPF items: CIP Identity + Socket Address
    response->item_count = 2;
    response->items = calloc(2, sizeof(struct eip_cpf_item_header*));
    if (!response->items) {
        error("Failed to allocate CPF items array");
        free(response);
        return BUF_ERR_NO_RESOURCES;
    }
    
    // Create CIP Identity Item
    eip_cpf_cip_identity_item *identity_item = calloc(1, sizeof(eip_cpf_cip_identity_item));
    if (!identity_item) {
        error("Failed to allocate CIP Identity Item");
        free(response->items);
        free(response);
        return BUF_ERR_NO_RESOURCES;
    }
    
    identity_item->header.type_id = CPF_TYPE_ID_CIP_IDENTITY;
    identity_item->vendor_id = VENDOR_ID;
    identity_item->device_type = DEVICE_TYPE;
    identity_item->product_code = PRODUCT_CODE;
    identity_item->major_revision = MAJOR_REVISION;
    identity_item->minor_revision = MINOR_REVISION;
    identity_item->status = DEVICE_STATUS;
    identity_item->serial_number = SERIAL_NUMBER;
    identity_item->product_name_length = strlen(PRODUCT_NAME);
    identity_item->product_name = calloc(identity_item->product_name_length + 1, sizeof(uint8_t));
    if (!identity_item->product_name) {
        error("Failed to allocate product name");
        free(identity_item);
        free(response->items);
        free(response);
        return BUF_ERR_NO_RESOURCES;
    }
    strcpy((char*)identity_item->product_name, PRODUCT_NAME);
    
    response->items[0] = (struct eip_cpf_item_header*)identity_item;
    
    // Create Socket Address Item
    eip_cpf_socket_addr_item *socket_item = calloc(1, sizeof(eip_cpf_socket_addr_item));
    if (!socket_item) {
        error("Failed to allocate Socket Address Item");
        free(identity_item->product_name);
        free(identity_item);
        free(response->items);
        free(response);
        return BUF_ERR_NO_RESOURCES;
    }
    
    socket_item->header.type_id = CPF_TYPE_ID_SOCKET_ADDR;
    socket_item->sin_family = htons(2); // AF_INET in network byte order
    socket_item->sin_port = htons(44818); // EtherNet/IP TCP port in network byte order
    
    // Convert source IP to network byte order
    struct in_addr addr;
    if (inet_aton(source_ip, &addr) != 0) {
        socket_item->sin_addr = addr.s_addr; // Already in network byte order
    } else {
        warn("Invalid source IP address: %s, using 0.0.0.0", source_ip);
        socket_item->sin_addr = 0;
    }
    socket_item->sin_zero = 0;
    
    response->items[1] = (struct eip_cpf_item_header*)socket_item;
    
    // Allocate buffer and encode response
    buf_err_t result = buf_alloc(response_buf, 1024);
    if (result != BUF_OK) {
        error("Failed to allocate response buffer");
        eip_list_identity_response_dispose(response);
        return result;
    }
    
    result = eip_list_identity_response_encode(*response_buf, response);
    if (result != BUF_OK) {
        error("Failed to encode List Identity Response");
        buf_free(*response_buf);
        *response_buf = NULL;
        eip_list_identity_response_dispose(response);
        return result;
    }
    
    // Log the response
    eip_list_identity_response_log_info(response);
    
    // Cleanup
    eip_list_identity_response_dispose(response);
    
    trace("Successfully created List Identity Response");
    return BUF_OK;
}

//=============================================================================
// EVENT HANDLERS
//=============================================================================

/**
 * Handle UDP broadcast requests (List Identity)
 */
static void udp_broadcast_handler(const ev_event *event) {
    trace("UDP broadcast handler called, event type: %s", ev_event_string(event->type));
    
    switch (event->type) {
        case EV_EVENT_READ: {
            if (!event->data || !*event->data) {
                warn("Received UDP read event with no data");
                return;
            }
            
            buf *request_buf = *event->data;
            info("Received UDP packet from %s:%d, %zu bytes", 
                 event->remote_host, event->remote_port, request_buf->len);
            
            // Try to decode as List Identity Request
            eip_list_identity_request *request;
            buf_err_t result = eip_list_identity_request_decode(&request, request_buf);
            if (result != BUF_OK) {
                warn("Failed to decode List Identity Request: %d", result);
                buf_free(request_buf);
                return;
            }
            
            eip_list_identity_request_log_info(request);
            
            // Create and send response
            buf *response_buf;
            result = create_list_identity_response(&response_buf, "192.168.1.100", 44818);
            if (result != BUF_OK) {
                error("Failed to create List Identity Response");
                eip_list_identity_request_dispose(request);
                buf_free(request_buf);
                return;
            }
            
            // Send response back to requester
            result = ev_udp_send(event->sock, &response_buf, event->remote_host, event->remote_port);
            if (result != EV_OK) {
                error("Failed to send UDP response: %s", ev_err_string(result));
                buf_free(response_buf);
            } else {
                info("Sent List Identity Response to %s:%d", event->remote_host, event->remote_port);
            }
            
            // Cleanup
            eip_list_identity_request_dispose(request);
            buf_free(request_buf);
            break;
        }
        
        case EV_EVENT_ERROR:
            error("UDP socket error: %s", ev_err_string(event->error));
            break;
            
        case EV_EVENT_CLOSE:
            info("UDP socket closed");
            break;
            
        default:
            trace("Unhandled UDP event: %s", ev_event_string(event->type));
            break;
    }
}

/**
 * Handle TCP connections (Register Session, etc.)
 */
static void tcp_server_handler(const ev_event *event) {
    trace("TCP server handler called, event type: %s", ev_event_string(event->type));
    
    switch (event->type) {
        case EV_EVENT_ACCEPT:
            info("New TCP connection from %s:%d", event->remote_host, event->remote_port);
            // In a full implementation, we would set up a client-specific handler
            // For now, we just log the connection
            break;
            
        case EV_EVENT_READ: {
            if (!event->data || !*event->data) {
                warn("Received TCP read event with no data");
                return;
            }
            
            buf *request_buf = *event->data;
            info("Received TCP data from %s:%d, %zu bytes", 
                 event->remote_host, event->remote_port, request_buf->len);
            
            // In a full implementation, we would handle various EtherNet/IP commands
            // For now, we just log and close
            buf_free(request_buf);
            break;
        }
        
        case EV_EVENT_ERROR:
            error("TCP server error: %s", ev_err_string(event->error));
            break;
            
        case EV_EVENT_CLOSE:
            info("TCP server closed");
            break;
            
        default:
            trace("Unhandled TCP event: %s", ev_event_string(event->type));
            break;
    }
}

//=============================================================================
// MAIN FUNCTION
//=============================================================================

int main(int argc, char *argv[]) {
    info("Starting EtherNet/IP Server");
    
    // Set up signal handling
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Create event loop
    ev_loop_opts loop_opts = {
        .worker_threads = 2,
        .max_events = 64,
        .auto_start = true
    };
    
    ev_err result = ev_loop_create(&g_loop, &loop_opts);
    if (result != EV_OK) {
        error("Failed to create event loop: %s", ev_err_string(result));
        return 1;
    }
    
    // Create UDP socket for broadcast List Identity requests
    ev_udp_opts udp_opts = {
        .bind_host = "0.0.0.0",
        .bind_port = 2222,  // EtherNet/IP UDP port
        .callback = udp_broadcast_handler,
        .user_data = NULL,
        .broadcast = true,
        .reuse_addr = true,
        .read_buffer_size = 1024
    };
    
    ev_sock *udp_sock;
    result = ev_udp_create(g_loop, &udp_sock, &udp_opts);
    if (result != EV_OK) {
        error("Failed to create UDP socket: %s", ev_err_string(result));
        ev_loop_destroy(g_loop);
        return 1;
    }
    
    info("UDP socket listening on port 2222 for List Identity requests");
    
    // Create TCP server for EtherNet/IP connections
    ev_tcp_server_opts tcp_opts = {
        .bind_host = "0.0.0.0",
        .bind_port = 44818,  // EtherNet/IP TCP port
        .backlog = 10,
        .callback = tcp_server_handler,
        .user_data = NULL,
        .reuse_addr = true,
        .keep_alive = false,
        .read_buffer_size = 1024
    };
    
    ev_sock *tcp_server;
    result = ev_tcp_server_start(g_loop, &tcp_server, &tcp_opts);
    if (result != EV_OK) {
        error("Failed to start TCP server: %s", ev_err_string(result));
        ev_close(udp_sock);
        ev_loop_destroy(g_loop);
        return 1;
    }
    
    info("TCP server listening on port 44818 for EtherNet/IP connections");
    info("EtherNet/IP Server started successfully");
    info("Device Info: Vendor=0x%04X, Type=0x%04X, Product=0x%04X, Serial=0x%08X",
         VENDOR_ID, DEVICE_TYPE, PRODUCT_CODE, SERIAL_NUMBER);
    info("Product Name: %s", PRODUCT_NAME);
    info("Press Ctrl+C to stop...");
    
    // Wait for the event loop to finish
    result = ev_loop_wait(g_loop);
    if (result != EV_OK) {
        error("Event loop error: %s", ev_err_string(result));
    }
    
    // Cleanup
    info("Shutting down EtherNet/IP Server");
    ev_close(udp_sock);
    ev_close(tcp_server);
    ev_loop_destroy(g_loop);
    
    info("EtherNet/IP Server stopped");
    return 0;
}