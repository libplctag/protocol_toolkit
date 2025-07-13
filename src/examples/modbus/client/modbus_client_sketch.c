/* this is just a sketch of how it might work */

#include <modbus.h>
#include <ptk_log.h>
#include <ptk_utils.h>


static bool done = false;

void ctrlc_handler(void) {
    done = true;
}

int main(int argc, char *argv[]) {
    modbus_connection_t *conn = NULL;
    modbus_read_coils_req_t *req = NULL;
    modbus_read_coils_resp_t *resp = NULL;

    ptk_set_interrupt_handler(ctrlc_handler);

    process_command_line_args(argc, argv);

    // Initialize Modbus client
    conn = modbus_client_connect("127.0.0.1", 502);
    if(!conn) {
        error("Failed to connect to Modbus server: %s!", ptk_strerror(ptk_get_err()));
        return 1;
    }

    // Perform Modbus operations...
    while(!done) {
        req = modbus_read_coils_req_create(conn, 0, 10);
        if(!req) {
            error("Failed to create read coils request: %s!", ptk_strerror(ptk_get_err()));
            break;
        }

        req->starting_address = 0;
        req->quantity_of_coils = 10;

        modbus_read_coils_resp_t *resp = modbus_pdu_send(conn, req, 1000);
        if(!resp) {
            error("Failed to send read coils request: %s!", ptk_strerror(ptk_get_err()));
            ptk_free(req);
            break;
        }        

        for(size_t i = 0; i < resp->coil_values->len; i++) {
            bool value;
            modbus_bit_array_get(resp->coil_values, i, &value);
            info("Coil %zu: %s", i, value ? "ON" : "OFF");
        }

        ptk_free(resp);
    }

    if(req) ptk_free(req);
    if(resp) ptk_free(resp);

    modbus_close(conn);


    return 0;
}