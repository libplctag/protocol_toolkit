#include "../include/ethernetip.h"
#include <ptk_mem.h>
#include <ptk_sock.h>
#include <ptk_err.h>
#include <ptk_utils.h>
#include <string.h>

//=============================================================================
// INTERNAL CONNECTION STRUCTURE
//=============================================================================

struct eip_connection_t {
    ptk_sock *socket;           // Child of connection
    ptk_buf *io_buffer;         // Child of connection
    
    // Connection parameters
    char host[256];             // Target host (IP or hostname)
    uint16_t port;              // Target port (typically 44818)
    
    // Session state
    uint32_t session_handle;    // Current session handle (0 if no session)
    uint64_t next_sender_context; // For request/response matching
    
    // Transport type
    bool is_udp;                // true for UDP (discovery), false for TCP
    bool session_registered;    // true if session is active
};

//=============================================================================
// PROTOCOL CONSTANTS
//=============================================================================

#define EIP_DEFAULT_PORT 44818
#define EIP_REGISTER_SESSION_CMD 0x0065
#define EIP_UNREGISTER_SESSION_CMD 0x0066
#define EIP_UNCONNECTED_SEND_CMD 0x006F

// EIP header size
#define EIP_HEADER_SIZE 24

//=============================================================================
// INTERNAL HELPER FUNCTIONS
//=============================================================================

static ptk_err_t eip_register_session(eip_connection_t *conn) {
    if (!conn || !conn->socket || conn->session_registered) {
        return PTK_ERR_INVALID_ARGUMENT;
    }
    
    // Build register session request
    ptk_buf *request = ptk_buf_alloc(EIP_HEADER_SIZE + 4);
    if (!request) {
        return PTK_ERR_OUT_OF_MEMORY;
    }
    
    // EIP Header for Register Session
    ptk_err_t err = ptk_buf_serialize(request, PTK_BUF_LITTLE_ENDIAN,
                                   (uint16_t)EIP_REGISTER_SESSION_CMD,  // Command
                                   (uint16_t)4,                         // Length (version + options)
                                   (uint32_t)0,                         // Session Handle (0 for register)
                                   (uint32_t)0,                         // Status
                                   (uint64_t)conn->next_sender_context++, // Sender Context
                                   (uint32_t)0);                        // Options
    
    if (err != PTK_OK) {
        ptk_free(&request);
        return err;
    }
    
    // Register Session data: version=1, options=0
    err = ptk_buf_serialize(request, PTK_BUF_LITTLE_ENDIAN,
                           (uint16_t)1,  // Protocol version
                           (uint16_t)0); // Options flags
    
    if (err != PTK_OK) {
        ptk_free(&request);
        return err;
    }
    
    // Send request
    err = ptk_tcp_socket_send(conn->socket, request, 5000); // 5 second timeout
    ptk_free(&request);
    if (err != PTK_OK) {
        return err;
    }
    
    // Receive response
    ptk_buf *response = ptk_buf_alloc(512);
    if (!response) {
        return PTK_ERR_OUT_OF_MEMORY;
    }
    
    err = ptk_tcp_socket_recv(conn->socket, response, 5000); // 5 second timeout
    if (err != PTK_OK) {
        ptk_free(&response);
        return err;
    }
    
    // Parse response header
    uint16_t cmd, length;
    uint32_t session_handle, status;
    uint64_t sender_context;
    uint32_t options;
    
    err = ptk_buf_deserialize(response, false, PTK_BUF_LITTLE_ENDIAN,
                             &cmd, &length, &session_handle, &status,
                             &sender_context, &options);
    
    ptk_free(&response);
    
    if (err != PTK_OK) {
        return err;
    }
    
    if (cmd != EIP_REGISTER_SESSION_CMD || status != 0 || session_handle == 0) {
        return PTK_ERR_PROTOCOL_ERROR;
    }
    
    // Session successfully registered
    conn->session_handle = session_handle;
    conn->session_registered = true;
    
    return PTK_OK;
}

static ptk_err_t eip_unregister_session(eip_connection_t *conn) {
    if (!conn || !conn->socket || !conn->session_registered) {
        return PTK_OK; // Already unregistered
    }
    
    // Build unregister session request
    ptk_buf *request = ptk_buf_alloc(EIP_HEADER_SIZE);
    if (!request) {
        return PTK_ERR_OUT_OF_MEMORY;
    }
    
    ptk_err_t err = ptk_buf_serialize(request, PTK_BUF_LITTLE_ENDIAN,
                                   (uint16_t)EIP_UNREGISTER_SESSION_CMD, // Command
                                   (uint16_t)0,                          // Length
                                   (uint32_t)conn->session_handle,       // Session Handle
                                   (uint32_t)0,                          // Status
                                   (uint64_t)conn->next_sender_context++, // Sender Context
                                   (uint32_t)0);                         // Options
    
    if (err == PTK_OK) {
        ptk_tcp_socket_send(conn->socket, request, 5000); // 5 second timeout
    }
    
    ptk_free(&request);
    
    conn->session_handle = 0;
    conn->session_registered = false;
    
    return PTK_OK;
}

static void eip_connection_destructor(void *ptr) {
    eip_connection_t *conn = (eip_connection_t *)ptr;
    if (conn) {
        eip_unregister_session(conn);
        if (conn->socket) {
            ptk_socket_close(conn->socket);
        }
    }
}

//=============================================================================
// PUBLIC CONNECTION FUNCTIONS
//=============================================================================

eip_connection_t *eip_client_connect(const char *host, int port) {
    if (!host) {
        ptk_set_err(PTK_ERR_INVALID_ARGUMENT);
        return NULL;
    }
    
    if (port <= 0) {
        port = EIP_DEFAULT_PORT;
    }
    
    eip_connection_t *conn = ptk_alloc(sizeof(eip_connection_t), eip_connection_destructor);
    if (!conn) {
        ptk_set_err(PTK_ERR_OUT_OF_MEMORY);
        return NULL;
    }
    
    memset(conn, 0, sizeof(*conn));
    strncpy(conn->host, host, sizeof(conn->host) - 1);
    conn->port = (uint16_t)port;
    conn->next_sender_context = 1;
    conn->is_udp = false;
    
    // Create TCP socket
    ptk_address_t remote_addr;
    ptk_err_t err = ptk_address_create(&remote_addr, host, (uint16_t)port);
    if (err != PTK_OK) {
        ptk_set_err(err);
        ptk_free(&conn);
        return NULL;
    }
    
    conn->socket = ptk_tcp_socket_connect(&remote_addr, 5000); // 5 second timeout
    if (!conn->socket) {
        ptk_free(&conn);
        return NULL;
    }
    
    // Create I/O buffer
    conn->io_buffer = ptk_buf_alloc(4096);
    if (!conn->io_buffer) {
        ptk_set_err(PTK_ERR_OUT_OF_MEMORY);
        ptk_free(&conn);
        return NULL;
    }
    
    // Register EIP session
    err = eip_register_session(conn);
    if (err != PTK_OK) {
        ptk_set_err(err);
        ptk_free(&conn);
        return NULL;
    }
    
    return conn;
}

eip_connection_t *eip_client_connect_udp(const char *host, int port) {
    if (!host) {
        ptk_set_err(PTK_ERR_INVALID_ARGUMENT);
        return NULL;
    }
    
    if (port <= 0) {
        port = EIP_DEFAULT_PORT;
    }
    
    eip_connection_t *conn = ptk_alloc(sizeof(eip_connection_t), eip_connection_destructor);
    if (!conn) {
        ptk_set_err(PTK_ERR_OUT_OF_MEMORY);
        return NULL;
    }
    
    memset(conn, 0, sizeof(*conn));
    strncpy(conn->host, host, sizeof(conn->host) - 1);
    conn->port = (uint16_t)port;
    conn->next_sender_context = 1;
    conn->is_udp = true;
    conn->session_registered = false; // UDP doesn't use sessions
    
    // Create UDP socket
    ptk_address_t local_addr;
    ptk_err_t err = ptk_address_create_any(&local_addr, 0);
    if (err != PTK_OK) {
        ptk_set_err(err);
        ptk_free(&conn);
        return NULL;
    }
    
    conn->socket = ptk_udp_socket_create(&local_addr, true); // Enable broadcast
    if (!conn->socket) {
        ptk_free(&conn);
        return NULL;
    }
    
    // Create I/O buffer
    conn->io_buffer = ptk_buf_alloc(4096);
    if (!conn->io_buffer) {
        ptk_set_err(PTK_ERR_OUT_OF_MEMORY);
        ptk_free(&conn);
        return NULL;
    }
    
    return conn;
}

eip_connection_t *eip_server_listen(const char *host, int port, int backlog) {
    if (port <= 0) {
        port = EIP_DEFAULT_PORT;
    }
    
    eip_connection_t *conn = ptk_alloc(sizeof(eip_connection_t), eip_connection_destructor);
    if (!conn) {
        ptk_set_err(PTK_ERR_OUT_OF_MEMORY);
        return NULL;
    }
    
    memset(conn, 0, sizeof(*conn));
    if (host) {
        strncpy(conn->host, host, sizeof(conn->host) - 1);
    }
    conn->port = (uint16_t)port;
    conn->is_udp = false;
    
    // Create listening address
    ptk_address_t listen_addr;
    ptk_err_t err = host ? ptk_address_create(&listen_addr, host, (uint16_t)port)
                       : ptk_address_create_any(&listen_addr, (uint16_t)port);
    
    if (err != PTK_OK) {
        ptk_set_err(err);
        ptk_free(&conn);
        return NULL;
    }
    
    // Create TCP server socket
    conn->socket = ptk_tcp_socket_listen(&listen_addr, backlog > 0 ? backlog : 5);
    if (!conn->socket) {
        ptk_free(&conn);
        return NULL;
    }
    
    // Create I/O buffer
    conn->io_buffer = ptk_buf_alloc(4096);
    if (!conn->io_buffer) {
        ptk_set_err(PTK_ERR_OUT_OF_MEMORY);
        ptk_free(&conn);
        return NULL;
    }
    
    return conn;
}

ptk_err_t eip_abort(eip_connection_t *conn) {
    if (!conn || !conn->socket) {
        return PTK_ERR_INVALID_ARGUMENT;
    }
    
    return ptk_socket_abort(conn->socket);
}

ptk_err_t eip_signal(eip_connection_t *conn) {
    if (!conn || !conn->socket) {
        return PTK_ERR_INVALID_ARGUMENT;
    }
    
    return ptk_socket_signal(conn->socket);
}

ptk_err_t eip_wait_for_signal(eip_connection_t *conn, ptk_duration_ms timeout_ms) {
    if (!conn || !conn->socket) {
        return PTK_ERR_INVALID_ARGUMENT;
    }
    
    return ptk_socket_wait_for_signal(conn->socket, timeout_ms);
}

//=============================================================================
// INTERNAL CONNECTION ACCESS
//=============================================================================

ptk_sock *eip_connection_get_socket(eip_connection_t *conn) {
    return conn ? conn->socket : NULL;
}

ptk_buf *eip_connection_get_buffer(eip_connection_t *conn) {
    return conn ? conn->io_buffer : NULL;
}

uint32_t eip_connection_get_session_handle(eip_connection_t *conn) {
    return conn ? conn->session_handle : 0;
}

uint64_t eip_connection_get_next_sender_context(eip_connection_t *conn) {
    return conn ? conn->next_sender_context++ : 0;
}

bool eip_connection_is_udp(eip_connection_t *conn) {
    return conn ? conn->is_udp : false;
}

bool eip_connection_is_session_registered(eip_connection_t *conn) {
    return conn ? conn->session_registered : false;
}