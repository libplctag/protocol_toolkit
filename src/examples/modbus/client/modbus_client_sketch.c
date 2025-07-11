/* this is just a sketch of how it might work */

int main(int argc, char *argv[]) {

    process_command_line_args(argc, argv);

    // Allocate a shared buffer.
    ptk_buf_t *shared_buffer = ptk_buf_create(NULL, 1024);
    if(!shared_buffer) {
        error("Failed to allocate shared buffer\n");
        return 1;
    }

    ptk_socket_t *socket = ptk_socket_create(shared_buffer, PTK_SOCKET_TYPE_TCP);
    if(!socket) {
        error("Failed to create socket\n");
        ptk_free(shared_buffer);
        return 1;
    }

    err = pkt_socket_connect(socket, "127.0.0.1", 502, buffer, timeout_ms);
    if(err != PTK_OK) {
        error("Failed to connect socket: %d\n", err);
        ptk_free(socket);
        ptk_free(shared_buffer);
        return 1;
    }

    // socket takes ownership of the shared buffer, so we don't need to free it here.

    // Initialize Modbus client
    modbus_connection *conn = modbus_connect("127.0.0.1", 502, socket);
    if(!conn) {
        error("Failed to connect to Modbus server\n");
        ptk_free(socket);
        return 1;
    }

    // Perform Modbus operations...
    while(!done) {
        // Example: Read coils
        // the underlying socket calls ptk_free on the chain of structs, but not the shared buffer
        // this resets the buffer first.

        // blocks until the request is sent or the timeout occurs
        err = modbus_send_read_coils_req(conn, 0, 10, write_timeout_ms);
        if(err != PTK_OK) {
            error("Failed to send read coils request: %d\n", err);
            modbus_close(conn);
            return 1;
        }

        // Wait for response, blocking until a frame is received or the timeout occurs
        // resets the buffer, and reads the frame into it.
        modbuf_mbap_t *mbap = modbus_recv_frame(conn, read_timeout_ms);
        if(!mbap) {
            error("Failed to receive Modbus frame\n");
            modbus_close(conn);
            return 1;
        }

        switch(mbap->payload.mbus_base_pdu.function_code) {
            case MODBUS_FC_READ_COILS: handel_read_coils_response(conn, mbap->payload.modubus_read_coils_resp); break;
            case MODBUS_FC_READ_DISCRETE_INPUTS:
                handel_read_discrete_inputs_response(conn, mbap->payload.modubus_read_discrete_inputs_resp);
                break;
            case MODBUS_FC_READ_HOLDING_REGISTERS:
                handel_read_holding_registers_response(conn, mbap->payload.modubus_read_holding_registers_resp);
                break;
            case MODBUS_FC_READ_INPUT_REGISTERS:
                handel_read_input_registers_response(conn, mbap->payload.modubus_read_input_registers_resp);
                break;
            case MODBUS_FC_WRITE_SINGLE_COIL:
                handel_write_single_coil_response(conn, mbap->payload.modubus_write_single_coil_resp);
                break;
            case MODBUS_FC_WRITE_MULTIPLE_COILS:
                handel_write_multiple_coils_response(conn, mbap->payload.modubus_write_multiple_coils_resp);
                break;
            case MODBUS_FC_WRITE_SINGLE_REGISTER:
                handel_write_single_register_response(conn, mbap->payload.modubus_write_single_register_resp);
                break;
            case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
                handel_write_multiple_registers_response(conn, mbap->payload.modubus_write_multiple_registers_resp);
                break;
            default:
                error("Unknown function code: %d\n", mbap->payload.mbus_base_pdu.function_code);
                modbus_close(conn);
                return 1;
        }

        ptk_free(mbap);  // Free the received frame, and all associated data

        ptk_buf_reset(shared_buffer);  // Reset the buffer for the next operation
    }

    // Cleanup
    modbus_close(conn);
    return 0;
}