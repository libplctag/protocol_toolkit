#include <modbus.h>
#include <ptk_log.h>
#include <ptk_err.h>
#include <ptk_sock.h>
#include <ptk_mem.h>
#include <ptk_buf.h>
#include <string.h>

//=============================================================================
// MODBUS CONNECTION STRUCTURE
//=============================================================================

struct modbus_connection_t {
    ptk_sock *socket;
    ptk_address_t addr;
    uint8_t unit_id;
    bool is_server;
    uint16_t transaction_id;
};

//=============================================================================
// PRIVATE FUNCTIONS
//=============================================================================

/**
 * @brief Destructor for modbus connection
 *
 * @param ptr Pointer to the modbus connection to clean up
 */
static void modbus_connection_destructor(void *ptr) {
    info("destroying modbus connection");
    modbus_connection_t *conn = (modbus_connection_t *)ptr;
    if (conn && conn->socket) {
        ptk_free(&conn->socket);
    }
}

//=============================================================================
// PUBLIC FUNCTIONS
//=============================================================================

/**
 * @brief Connect to a Modbus TCP server
 *
 * @param host Hostname or IP address of the server
 * @param port Port number (typically 502 for Modbus TCP)
 * @return Valid connection on success, NULL on failure (check ptk_get_err())
 */
modbus_connection_t *modbus_client_connect(const char *host, int port) {
    info("connecting to modbus server %s:%d", host, port);
    
    if (!host || port <= 0 || port > 65535) {
        warn("invalid host or port parameters");
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return NULL;
    }

    ptk_address_t addr;
    if (ptk_address_init(&addr, host, (uint16_t)port) != PTK_OK) {
        warn("failed to create address for %s:%d", host, port);
        return NULL;
    }

    ptk_sock *socket = ptk_tcp_socket_connect(&addr, 5000);
    if (!socket) {
        warn("failed to connect to %s:%d", host, port);
        return NULL;
    }

    modbus_connection_t *conn = ptk_alloc(sizeof(modbus_connection_t), modbus_connection_destructor);
    if (!conn) {
        ptk_free(&socket);
        error("failed to allocate modbus connection");
        return NULL;
    }

    conn->socket = socket;
    conn->addr = addr;
    conn->unit_id = 1;
    conn->is_server = false;
    conn->transaction_id = 1;

    info("successfully connected to modbus server %s:%d", host, port);
    return conn;
}

/**
 * @brief Create a Modbus TCP server listener
 *
 * @param host Hostname or IP address to bind to (NULL for all interfaces)
 * @param port Port number to listen on
 * @param unit_id Unit ID for this server
 * @param backlog Maximum number of pending connections
 * @return Valid server connection on success, NULL on failure (check ptk_get_err())
 */
modbus_connection_t *modbus_server_listen(const char *host, int port, uint8_t unit_id, int backlog) {
    info("creating modbus server listener on %s:%d", host ? host : "*", port);
    
    if (port <= 0 || port > 65535 || backlog < 1) {
        warn("invalid port or backlog parameters");
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return NULL;
    }

    ptk_address_t addr;
    ptk_err_t err;
    if (host) {
        err = ptk_address_init(&addr, host, (uint16_t)port);
    } else {
        err = ptk_address_init_any(&addr, (uint16_t)port);
    }
    
    if (err != PTK_OK) {
        warn("failed to create bind address");
        return NULL;
    }

    ptk_sock *socket = ptk_tcp_socket_listen(&addr, backlog);
    if (!socket) {
        warn("failed to create listening socket");
        return NULL;
    }

    modbus_connection_t *conn = ptk_alloc(sizeof(modbus_connection_t), modbus_connection_destructor);
    if (!conn) {
        ptk_free(&socket);
        error("failed to allocate modbus server connection");
        return NULL;
    }

    conn->socket = socket;
    conn->addr = addr;
    conn->unit_id = unit_id;
    conn->is_server = true;
    conn->transaction_id = 0;

    info("modbus server listening on %s:%d", host ? host : "*", port);
    return conn;
}

/**
 * @brief Signal a Modbus connection
 *
 * @param conn Connection to signal
 * @return PTK_OK on success, error code on failure
 */
ptk_err_t modbus_signal(modbus_connection_t *conn) {
    info("signaling modbus connection");
    
    if (!conn) {
        warn("attempted to signal null connection");
        return PTK_ERR_INVALID_PARAM;
    }

    if (conn->socket) {
        return ptk_socket_signal(conn->socket);
    }

    return PTK_OK;
}

/**
 * @brief Wait for signal on a Modbus connection
 *
 * @param conn Connection to wait on
 * @param timeout_ms Timeout in milliseconds
 * @return PTK_OK on success, error code on failure
 */
ptk_err_t modbus_wait_for_signal(modbus_connection_t *conn, ptk_duration_ms timeout_ms) {
    info("waiting for signal on modbus connection");
    
    if (!conn) {
        warn("attempted to wait on null connection");
        return PTK_ERR_INVALID_PARAM;
    }

    if (conn->socket) {
        return ptk_socket_wait(conn->socket, timeout_ms);
    }

    return PTK_ERR_INVALID_PARAM;
}

/**
 * @brief Abort all operations on a Modbus connection
 *
 * @param conn Connection to abort
 * @return PTK_OK on success, error code on failure
 */
ptk_err_t modbus_abort(modbus_connection_t *conn) {
    info("aborting modbus connection");
    
    if (!conn) {
        warn("attempted to abort null connection");
        return PTK_ERR_INVALID_PARAM;
    }

    if (conn->socket) {
        return ptk_socket_abort(conn->socket);
    }

    return PTK_OK;
}