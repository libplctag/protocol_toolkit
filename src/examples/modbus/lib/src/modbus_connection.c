#include "modbus_internal.h"
#include <ptk_log.h>

//=============================================================================
// CONNECTION MANAGEMENT
//=============================================================================

modbus_connection *modbus_open_client(void *parent, ptk_address_t *addr, uint8_t unit_id) {
    if(!addr) {
        error("Invalid address provided");
        return NULL;
    }

    modbus_connection *conn = ptk_alloc(parent, sizeof(modbus_connection), NULL);
    if(!conn) {
        error("Failed to allocate connection structure");
        return NULL;
    }

    conn->parent = parent;
    conn->unit_id = unit_id;
    conn->next_transaction_id = 1;

    // Create socket as child of connection
    conn->socket = ptk_socket_create_client(conn, addr);
    if(!conn->socket) {
        error("Failed to create client socket");
        ptk_free(conn);
        return NULL;
    }

    // Create buffers as children of connection
    conn->rx_buffer = ptk_buf_create(conn, 1024);  // 1KB receive buffer
    if(!conn->rx_buffer) {
        error("Failed to create receive buffer");
        ptk_free(conn);
        return NULL;
    }

    conn->tx_buffer = ptk_buf_create(conn, 1024);  // 1KB transmit buffer
    if(!conn->tx_buffer) {
        error("Failed to create transmit buffer");
        ptk_free(conn);
        return NULL;
    }

    debug("Created Modbus client connection to unit %u", unit_id);
    return conn;
}

modbus_connection *modbus_open_server(void *parent, ptk_address_t *addr, uint8_t unit_id) {
    if(!addr) {
        error("Invalid address provided");
        return NULL;
    }

    modbus_connection *conn = ptk_alloc(parent, sizeof(modbus_connection), NULL);
    if(!conn) {
        error("Failed to allocate connection structure");
        return NULL;
    }

    conn->parent = parent;
    conn->unit_id = unit_id;
    conn->next_transaction_id = 0;  // Servers don't generate transaction IDs

    // Create server socket as child of connection
    conn->socket = ptk_socket_create_server(conn, addr);
    if(!conn->socket) {
        error("Failed to create server socket");
        ptk_free(conn);
        return NULL;
    }

    // Create buffers as children of connection
    conn->rx_buffer = ptk_buf_create(conn, 1024);
    if(!conn->rx_buffer) {
        error("Failed to create receive buffer");
        ptk_free(conn);
        return NULL;
    }

    conn->tx_buffer = ptk_buf_create(conn, 1024);
    if(!conn->tx_buffer) {
        error("Failed to create transmit buffer");
        ptk_free(conn);
        return NULL;
    }

    debug("Created Modbus server connection for unit %u", unit_id);
    return conn;
}

ptk_err modbus_close(modbus_connection *conn) {
    if(!conn) { return PTK_ERR_NULL_PTR; }

    debug("Closing Modbus connection");

    // Close socket if open
    if(conn->socket) { ptk_socket_close(conn->socket); }

    // Free the connection (this will automatically free all children)
    ptk_free(conn);

    return PTK_OK;
}

//=============================================================================
// HIGH-LEVEL CLIENT API
//=============================================================================

static ptk_err modbus_client_send_request_and_receive_response(modbus_connection *conn, modbus_mbap_t *request,
                                                               modbus_mbap_t **response) {
    if(!conn || !request || !response) { return PTK_ERR_NULL_PTR; }

    // Set transaction ID
    request->transaction_id = conn->next_transaction_id++;
    if(conn->next_transaction_id == 0) {
        conn->next_transaction_id = 1;  // Skip 0
    }

    // Clear transmit buffer
    ptk_buf_set_start(conn->tx_buffer, 0);
    ptk_buf_set_end(conn->tx_buffer, 0);

    // Serialize request
    ptk_err err = modbus_mbap_serialize(conn->tx_buffer, &request->base.buf_base);
    if(err != PTK_OK) {
        error("Failed to serialize request");
        return err;
    }

    // Send request
    err = ptk_socket_send(conn->socket, conn->tx_buffer);
    if(err != PTK_OK) {
        error("Failed to send request");
        return err;
    }

    // Receive response
    err = ptk_socket_receive(conn->socket, conn->rx_buffer);
    if(err != PTK_OK) {
        error("Failed to receive response");
        return err;
    }

    // Create response MBAP
    *response = modbus_mbap_create(conn);
    if(!*response) {
        error("Failed to create response MBAP");
        return PTK_ERR_NO_RESOURCES;
    }

    // Deserialize response
    err = modbus_mbap_deserialize(conn->rx_buffer, &(*response)->base.buf_base);
    if(err != PTK_OK) {
        error("Failed to deserialize response");
        ptk_free(*response);
        *response = NULL;
        return err;
    }

    // Validate transaction ID
    if((*response)->transaction_id != request->transaction_id) {
        error("Transaction ID mismatch: expected %u, got %u", request->transaction_id, (*response)->transaction_id);
        ptk_free(*response);
        *response = NULL;
        return PTK_ERR_INVALID_PARAM;
    }

    return PTK_OK;
}

ptk_err modbus_client_read_coils(modbus_connection *conn, uint16_t starting_address, uint16_t quantity,
                                 modbus_byte_array_t **coil_status) {
    if(!conn || !coil_status) { return PTK_ERR_NULL_PTR; }

    // Create request MBAP
    modbus_mbap_t *request = modbus_mbap_create(conn);
    if(!request) { return PTK_ERR_NO_RESOURCES; }

    request->protocol_id = 0;
    request->unit_id = conn->unit_id;

    // Create read coils request
    request->pdu.read_coils_req = modbus_read_coils_req_create(request);
    if(!request->pdu.read_coils_req) {
        ptk_free(request);
        return PTK_ERR_NO_RESOURCES;
    }

    request->pdu.read_coils_req->starting_address = starting_address;
    request->pdu.read_coils_req->quantity_of_coils = quantity;

    // Send request and receive response
    modbus_mbap_t *response = NULL;
    ptk_err err = modbus_client_send_request_and_receive_response(conn, request, &response);

    ptk_free(request);  // Clean up request

    if(err != PTK_OK) { return err; }

    // Check for exception response
    if(response->pdu.exception_resp) {
        error("Received exception response: %s", modbus_get_exception_description(response->pdu.exception_resp->exception_code));
        ptk_free(response);
        return PTK_ERR_INVALID_PARAM;
    }

    // Extract coil status
    if(!response->pdu.read_coils_resp) {
        error("Invalid response PDU type");
        ptk_free(response);
        return PTK_ERR_INVALID_PARAM;
    }

    *coil_status = response->pdu.read_coils_resp->coil_status;
    response->pdu.read_coils_resp->coil_status = NULL;  // Transfer ownership

    ptk_free(response);
    return PTK_OK;
}

ptk_err modbus_client_read_discrete_inputs(modbus_connection *conn, uint16_t starting_address, uint16_t quantity,
                                           modbus_byte_array_t **input_status) {
    if(!conn || !input_status) { return PTK_ERR_NULL_PTR; }

    // Similar implementation to read_coils...
    // TODO: Implement similar to modbus_client_read_coils

    return PTK_ERR_NOT_IMPLEMENTED;
}

ptk_err modbus_client_read_holding_registers(modbus_connection *conn, uint16_t starting_address, uint16_t quantity,
                                             modbus_register_array_t **register_values) {
    if(!conn || !register_values) { return PTK_ERR_NULL_PTR; }

    // Similar implementation to read_coils but for registers...
    // TODO: Implement similar to modbus_client_read_coils

    return PTK_ERR_NOT_IMPLEMENTED;
}

ptk_err modbus_client_read_input_registers(modbus_connection *conn, uint16_t starting_address, uint16_t quantity,
                                           modbus_register_array_t **register_values) {
    if(!conn || !register_values) { return PTK_ERR_NULL_PTR; }

    // Similar implementation to read_coils but for input registers...
    // TODO: Implement similar to modbus_client_read_coils

    return PTK_ERR_NOT_IMPLEMENTED;
}

ptk_err modbus_client_write_single_coil(modbus_connection *conn, uint16_t address, bool value) {
    if(!conn) { return PTK_ERR_NULL_PTR; }

    // Create request MBAP
    modbus_mbap_t *request = modbus_mbap_create(conn);
    if(!request) { return PTK_ERR_NO_RESOURCES; }

    request->protocol_id = 0;
    request->unit_id = conn->unit_id;

    // Create write single coil request
    request->pdu.write_single_coil_req = modbus_write_single_coil_req_create(request);
    if(!request->pdu.write_single_coil_req) {
        ptk_free(request);
        return PTK_ERR_NO_RESOURCES;
    }

    request->pdu.write_single_coil_req->output_address = address;
    request->pdu.write_single_coil_req->output_value = modbus_bool_to_coil_value(value);

    // Send request and receive response
    modbus_mbap_t *response = NULL;
    ptk_err err = modbus_client_send_request_and_receive_response(conn, request, &response);

    ptk_free(request);

    if(err != PTK_OK) { return err; }

    // Check for exception response
    if(response->pdu.exception_resp) {
        error("Received exception response: %s", modbus_get_exception_description(response->pdu.exception_resp->exception_code));
        ptk_free(response);
        return PTK_ERR_INVALID_PARAM;
    }

    // Validate echo response
    if(!response->pdu.write_single_coil_resp) {
        error("Invalid response PDU type");
        ptk_free(response);
        return PTK_ERR_INVALID_PARAM;
    }

    if(response->pdu.write_single_coil_resp->output_address != address
       || response->pdu.write_single_coil_resp->output_value != modbus_bool_to_coil_value(value)) {
        error("Response does not match request");
        ptk_free(response);
        return PTK_ERR_INVALID_PARAM;
    }

    ptk_free(response);
    return PTK_OK;
}

ptk_err modbus_client_write_single_register(modbus_connection *conn, uint16_t address, uint16_t value) {
    // Similar implementation to write_single_coil...
    // TODO: Implement similar to modbus_client_write_single_coil

    return PTK_ERR_NOT_IMPLEMENTED;
}

ptk_err modbus_client_write_multiple_coils(modbus_connection *conn, uint16_t starting_address,
                                           const modbus_byte_array_t *coil_values) {
    // Similar implementation but for multiple coils...
    // TODO: Implement

    return PTK_ERR_NOT_IMPLEMENTED;
}

ptk_err modbus_client_write_multiple_registers(modbus_connection *conn, uint16_t starting_address,
                                               const modbus_register_array_t *register_values) {
    // Similar implementation but for multiple registers...
    // TODO: Implement

    return PTK_ERR_NOT_IMPLEMENTED;
}

//=============================================================================
// SERVER API STUBS
//=============================================================================

ptk_err modbus_server_set_handlers(modbus_connection *conn, const modbus_server_handlers_t *handlers) {
    // TODO: Implement server handler registration
    return PTK_ERR_NOT_IMPLEMENTED;
}

ptk_err modbus_server_process_request(modbus_connection *conn) {
    // TODO: Implement server request processing
    return PTK_ERR_NOT_IMPLEMENTED;
}

modbus_connection *modbus_server_accept_connection(modbus_connection *server_conn) {
    // TODO: Implement server connection acceptance
    return NULL;
}
