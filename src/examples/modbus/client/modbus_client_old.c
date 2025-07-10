#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <modbus.h>
#include <ptk_alloc.h>
#include <ptk_buf.h>
#include <ptk_err.h>
#include <ptk_log.h>
#include <ptk_socket.h>
#include <ptk_utils.h>

//=============================================================================
// CONSTANTS AND DEFAULTS
//=============================================================================

#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_PORT 502
#define BUFFER_SIZE 1024

//=============================================================================
// COMMAND LINE OPTIONS
//=============================================================================

typedef enum {
    OP_READ_HOLDING,
    OP_READ_INPUT,
    OP_READ_COILS,
    OP_READ_DISCRETE,
    OP_WRITE_HOLDING,
    OP_WRITE_COIL,
    OP_WRITE_HOLDING_MULTIPLE,
    OP_WRITE_COILS_MULTIPLE
} operation_type;

typedef struct {
    char host[256];
    uint16_t port;
    operation_type operation;
    uint16_t start_address;
    uint16_t count;
    uint16_t *write_values;
    bool *write_coil_values;
    size_t write_values_count;
    uint8_t unit_id;
    bool verbose;
} client_config_t;

void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS] OPERATION\n", program_name);
    printf("Modbus TCP Client\n\n");

    printf("Connection Options:\n");
    printf("  -h, --host=HOST          Modbus server host (default: %s)\n", DEFAULT_HOST);
    printf("  -p, --port=PORT          Modbus server port (default: %d)\n", DEFAULT_PORT);
    printf("  -u, --unit-id=ID         Unit ID (default: 1)\n");
    printf("  -v, --verbose            Verbose output\n");
    printf("      --help               Show this help message\n\n");

    printf("Operations:\n");
    printf("Read Operations:\n");
    printf("  --read-holding=ADDR[,COUNT]     Read holding register(s) starting at ADDR\n");
    printf("  --read-input=ADDR[,COUNT]       Read input register(s) starting at ADDR\n");
    printf("  --read-coils=ADDR[,COUNT]       Read coil(s) starting at ADDR\n");
    printf("  --read-discrete=ADDR[,COUNT]    Read discrete input(s) starting at ADDR\n\n");

    printf("Write Operations:\n");
    printf("  --write-holding=ADDR,VALUE      Write single holding register\n");
    printf("  --write-coil=ADDR,VALUE         Write single coil (0 or 1)\n");
    printf("  --write-holdings=ADDR,VAL1,VAL2,...  Write multiple holding registers\n");
    printf("  --write-coils=ADDR,VAL1,VAL2,...     Write multiple coils (0 or 1)\n\n");

    printf("Examples:\n");
    printf("  %s --read-holding=100           Read holding register 100\n", program_name);
    printf("  %s --read-holding=100,10        Read 10 holding registers starting at 100\n", program_name);
    printf("  %s --write-holding=100,1234     Write value 1234 to holding register 100\n", program_name);
    printf("  %s --write-holdings=100,1,2,3   Write values 1,2,3 to registers 100,101,102\n", program_name);
    printf("  %s -h 192.168.1.100 --read-coils=0,8  Read 8 coils from address 0\n", program_name);
}

int parse_address_and_count(const char *arg, uint16_t *address, uint16_t *count) {
    char *endptr;
    *address = (uint16_t)strtoul(arg, &endptr, 10);

    if(*endptr == '\0') {
        *count = 1;  // Default count
        return 0;
    } else if(*endptr == ',') {
        *count = (uint16_t)strtoul(endptr + 1, &endptr, 10);
        if(*endptr == '\0' && *count > 0) { return 0; }
    }

    return -1;
}

int parse_write_values(const char *arg, uint16_t *address, uint16_t **values, size_t *count) {
    char *arg_copy = strdup(arg);
    if(!arg_copy) { return -1; }

    char *token = strtok(arg_copy, ",");
    if(!token) {
        free(arg_copy);
        return -1;
    }

    *address = (uint16_t)strtoul(token, NULL, 10);

    // Count the values
    size_t value_count = 0;
    char *temp_copy = strdup(arg);
    char *temp_token = strtok(temp_copy, ",");
    temp_token = strtok(NULL, ",");  // Skip address
    while(temp_token) {
        value_count++;
        temp_token = strtok(NULL, ",");
    }
    free(temp_copy);

    if(value_count == 0) {
        free(arg_copy);
        return -1;
    }

    *values = malloc(value_count * sizeof(uint16_t));
    if(!*values) {
        free(arg_copy);
        return -1;
    }

    // Parse the values
    strcpy(arg_copy, arg);
    token = strtok(arg_copy, ",");
    token = strtok(NULL, ",");  // Skip address

    for(size_t i = 0; i < value_count && token; i++) {
        (*values)[i] = (uint16_t)strtoul(token, NULL, 10);
        token = strtok(NULL, ",");
    }

    *count = value_count;
    free(arg_copy);
    return 0;
}

int parse_write_coil_values(const char *arg, uint16_t *address, bool **values, size_t *count) {
    char *arg_copy = strdup(arg);
    if(!arg_copy) { return -1; }

    char *token = strtok(arg_copy, ",");
    if(!token) {
        free(arg_copy);
        return -1;
    }

    *address = (uint16_t)strtoul(token, NULL, 10);

    // Count the values
    size_t value_count = 0;
    char *temp_copy = strdup(arg);
    char *temp_token = strtok(temp_copy, ",");
    temp_token = strtok(NULL, ",");  // Skip address
    while(temp_token) {
        value_count++;
        temp_token = strtok(NULL, ",");
    }
    free(temp_copy);

    if(value_count == 0) {
        free(arg_copy);
        return -1;
    }

    *values = malloc(value_count * sizeof(bool));
    if(!*values) {
        free(arg_copy);
        return -1;
    }

    // Parse the values
    strcpy(arg_copy, arg);
    token = strtok(arg_copy, ",");
    token = strtok(NULL, ",");  // Skip address

    for(size_t i = 0; i < value_count && token; i++) {
        (*values)[i] = (strtoul(token, NULL, 10) != 0);
        token = strtok(NULL, ",");
    }

    *count = value_count;
    free(arg_copy);
    return 0;
}

int parse_arguments(int argc, char *argv[], client_config_t *config) {
    // Set defaults
    strncpy(config->host, DEFAULT_HOST, sizeof(config->host) - 1);
    config->port = DEFAULT_PORT;
    config->unit_id = 1;
    config->verbose = false;
    config->write_values = NULL;
    config->write_coil_values = NULL;
    config->write_values_count = 0;

    bool operation_set = false;

    static struct option long_options[] = {{"host", required_argument, 0, 'h'},
                                           {"port", required_argument, 0, 'p'},
                                           {"unit-id", required_argument, 0, 'u'},
                                           {"verbose", no_argument, 0, 'v'},
                                           {"read-holding", required_argument, 0, 1001},
                                           {"read-input", required_argument, 0, 1002},
                                           {"read-coils", required_argument, 0, 1003},
                                           {"read-discrete", required_argument, 0, 1004},
                                           {"write-holding", required_argument, 0, 1005},
                                           {"write-coil", required_argument, 0, 1006},
                                           {"write-holdings", required_argument, 0, 1007},
                                           {"write-coils", required_argument, 0, 1008},
                                           {"help", no_argument, 0, '?'},
                                           {0, 0, 0, 0}};

    int c;
    while((c = getopt_long(argc, argv, "h:p:u:v", long_options, NULL)) != -1) {
        switch(c) {
            case 'h': strncpy(config->host, optarg, sizeof(config->host) - 1); break;
            case 'p': {
                long val = strtol(optarg, NULL, 10);
                if(val < 1 || val > 65535) {
                    error("Invalid port number: %s", optarg);
                    return -1;
                }
                config->port = (uint16_t)val;
                break;
            }
            case 'u': {
                long val = strtol(optarg, NULL, 10);
                if(val < 0 || val > 255) {
                    error("Invalid unit ID: %s", optarg);
                    return -1;
                }
                config->unit_id = (uint8_t)val;
                break;
            }
            case 'v': config->verbose = true; break;
            case 1001:  // read-holding
                if(operation_set) {
                    error("Only one operation can be specified");
                    return -1;
                }
                config->operation = OP_READ_HOLDING;
                if(parse_address_and_count(optarg, &config->start_address, &config->count) != 0) {
                    error("Invalid address/count format: %s", optarg);
                    return -1;
                }
                operation_set = true;
                break;
            case 1002:  // read-input
                if(operation_set) {
                    error("Only one operation can be specified");
                    return -1;
                }
                config->operation = OP_READ_INPUT;
                if(parse_address_and_count(optarg, &config->start_address, &config->count) != 0) {
                    error("Invalid address/count format: %s", optarg);
                    return -1;
                }
                operation_set = true;
                break;
            case 1003:  // read-coils
                if(operation_set) {
                    error("Only one operation can be specified");
                    return -1;
                }
                config->operation = OP_READ_COILS;
                if(parse_address_and_count(optarg, &config->start_address, &config->count) != 0) {
                    error("Invalid address/count format: %s", optarg);
                    return -1;
                }
                operation_set = true;
                break;
            case 1004:  // read-discrete
                if(operation_set) {
                    error("Only one operation can be specified");
                    return -1;
                }
                config->operation = OP_READ_DISCRETE;
                if(parse_address_and_count(optarg, &config->start_address, &config->count) != 0) {
                    error("Invalid address/count format: %s", optarg);
                    return -1;
                }
                operation_set = true;
                break;
            case 1005:  // write-holding
                if(operation_set) {
                    error("Only one operation can be specified");
                    return -1;
                }
                config->operation = OP_WRITE_HOLDING;
                if(parse_write_values(optarg, &config->start_address, &config->write_values, &config->write_values_count) != 0
                   || config->write_values_count != 1) {
                    error("Invalid write format: %s (expected ADDR,VALUE)", optarg);
                    return -1;
                }
                operation_set = true;
                break;
            case 1006:  // write-coil
                if(operation_set) {
                    error("Only one operation can be specified");
                    return -1;
                }
                config->operation = OP_WRITE_COIL;
                if(parse_write_coil_values(optarg, &config->start_address, &config->write_coil_values,
                                           &config->write_values_count)
                       != 0
                   || config->write_values_count != 1) {
                    error("Invalid write format: %s (expected ADDR,VALUE)", optarg);
                    return -1;
                }
                operation_set = true;
                break;
            case 1007:  // write-holdings
                if(operation_set) {
                    error("Only one operation can be specified");
                    return -1;
                }
                config->operation = OP_WRITE_HOLDING_MULTIPLE;
                if(parse_write_values(optarg, &config->start_address, &config->write_values, &config->write_values_count) != 0) {
                    error("Invalid write format: %s", optarg);
                    return -1;
                }
                operation_set = true;
                break;
            case 1008:  // write-coils
                if(operation_set) {
                    error("Only one operation can be specified");
                    return -1;
                }
                config->operation = OP_WRITE_COILS_MULTIPLE;
                if(parse_write_coil_values(optarg, &config->start_address, &config->write_coil_values,
                                           &config->write_values_count)
                   != 0) {
                    error("Invalid write format: %s", optarg);
                    return -1;
                }
                operation_set = true;
                break;
            case '?': print_usage(argv[0]); return 1;
            default: error("Unknown option"); return -1;
        }
    }

    if(!operation_set) {
        error("No operation specified. Use --help for usage information.");
        return -1;
    }

    return 0;
}

//=============================================================================
// MODBUS OPERATIONS
//=============================================================================

ptk_err execute_read_holding_registers(modbus_connection *conn, const client_config_t *config) {
    ptk_err err;

    if(config->count == 1) {
        // Single register read
        uint16_t value;
        err = client_send_read_holding_register_req(conn, config->start_address);
        if(err != PTK_OK) {
            error("Failed to send read holding register request");
            return err;
        }

        err = client_recv_read_read_holding_register_resp(conn, &value);
        if(err != PTK_OK) {
            error("Failed to receive read holding register response");
            return err;
        }

        printf("Holding register %d: %d (0x%04X)\n", config->start_address, value, value);
    } else {
        // Multiple registers read
        modbus_register_array_t *values;
        err = client_send_read_holding_registers_req(conn, config->start_address, config->count);
        if(err != PTK_OK) {
            error("Failed to send read holding registers request");
            return err;
        }

        err = client_recv_read_read_holding_registers_resp(conn, &values);
        if(err != PTK_OK) {
            error("Failed to receive read holding registers response");
            return err;
        }

        printf("Holding registers starting at %d:\n", config->start_address);
        for(uint16_t i = 0; i < config->count; i++) {
            uint16_t value;
            if(modbus_register_array_get(values, i, &value) == PTK_OK) {
                printf("  [%d]: %d (0x%04X)\n", config->start_address + i, value, value);
            }
        }

        modbus_register_array_dispose(values);
        // Note: The array structure itself is handled by the API
    }

    return PTK_OK;
}

ptk_err execute_read_input_registers(modbus_connection *conn, const client_config_t *config) {
    ptk_err err;

    if(config->count == 1) {
        // Single register read
        uint16_t value;
        err = client_send_read_input_register_req(conn, config->start_address);
        if(err != PTK_OK) {
            error("Failed to send read input register request");
            return err;
        }

        err = client_recv_read_input_register_resp(conn, &value);
        if(err != PTK_OK) {
            error("Failed to receive read input register response");
            return err;
        }

        printf("Input register %d: %d (0x%04X)\n", config->start_address, value, value);
    } else {
        // Multiple registers read
        modbus_register_array_t *values;
        err = client_send_read_input_registers_req(conn, config->start_address, config->count);
        if(err != PTK_OK) {
            error("Failed to send read input registers request");
            return err;
        }

        err = client_recv_read_input_registers_resp(conn, &values);
        if(err != PTK_OK) {
            error("Failed to receive read input registers response");
            return err;
        }

        printf("Input registers starting at %d:\n", config->start_address);
        for(uint16_t i = 0; i < config->count; i++) {
            uint16_t value;
            if(modbus_register_array_get(values, i, &value) == PTK_OK) {
                printf("  [%d]: %d (0x%04X)\n", config->start_address + i, value, value);
            }
        }

        modbus_register_array_dispose(values);
    }

    return PTK_OK;
}

ptk_err execute_read_coils(modbus_connection *conn, const client_config_t *config) {
    ptk_err err;

    if(config->count == 1) {
        // Single coil read
        bool value;
        err = client_send_read_coil_req(conn, config->start_address);
        if(err != PTK_OK) {
            error("Failed to send read coil request");
            return err;
        }

        err = client_recv_read_coil_resp(conn, &value);
        if(err != PTK_OK) {
            error("Failed to receive read coil response");
            return err;
        }

        printf("Coil %d: %s\n", config->start_address, value ? "ON" : "OFF");
    } else {
        // Multiple coils read
        modbus_bool_array_t *values;
        err = client_send_read_coils_req(conn, config->start_address, config->count);
        if(err != PTK_OK) {
            error("Failed to send read coils request");
            return err;
        }

        err = client_recv_read_coils_resp(conn, &values);
        if(err != PTK_OK) {
            error("Failed to receive read coils response");
            return err;
        }

        printf("Coils starting at %d:\n", config->start_address);
        for(uint16_t i = 0; i < config->count; i++) {
            bool value;
            if(modbus_bool_array_get(values, i, &value) == PTK_OK) {
                printf("  [%d]: %s\n", config->start_address + i, value ? "ON" : "OFF");
            }
        }

        modbus_bool_array_dispose(values);
    }

    return PTK_OK;
}

ptk_err execute_read_discrete_inputs(modbus_connection *conn, const client_config_t *config) {
    ptk_err err;

    if(config->count == 1) {
        // Single discrete input read
        bool value;
        err = client_send_read_discrete_input_req(conn, config->start_address);
        if(err != PTK_OK) {
            error("Failed to send read discrete input request");
            return err;
        }

        err = client_recv_read_discrete_input_resp(conn, &value);
        if(err != PTK_OK) {
            error("Failed to receive read discrete input response");
            return err;
        }

        printf("Discrete input %d: %s\n", config->start_address, value ? "ON" : "OFF");
    } else {
        // Multiple discrete inputs read
        modbus_bool_array_t *values;
        err = client_send_read_discrete_inputs_req(conn, config->start_address, config->count);
        if(err != PTK_OK) {
            error("Failed to send read discrete inputs request");
            return err;
        }

        err = client_recv_read_discrete_inputs_resp(conn, &values);
        if(err != PTK_OK) {
            error("Failed to receive read discrete inputs response");
            return err;
        }

        printf("Discrete inputs starting at %d:\n", config->start_address);
        for(uint16_t i = 0; i < config->count; i++) {
            bool value;
            if(modbus_bool_array_get(values, i, &value) == PTK_OK) {
                printf("  [%d]: %s\n", config->start_address + i, value ? "ON" : "OFF");
            }
        }

        modbus_bool_array_dispose(values);
    }

    return PTK_OK;
}

ptk_err execute_write_holding_register(modbus_connection *conn, const client_config_t *config, ptk_allocator_t *allocator) {
    ptk_err err;

    if(config->write_values_count == 1) {
        // Single register write
        err = client_send_write_holding_register_req(conn, config->start_address, config->write_values[0]);
        if(err != PTK_OK) {
            error("Failed to send write holding register request");
            return err;
        }

        err = client_recv_write_holding_register_resp(conn);
        if(err != PTK_OK) {
            error("Failed to receive write holding register response");
            return err;
        }

        printf("Successfully wrote %d to holding register %d\n", config->write_values[0], config->start_address);
    } else {
        // Multiple registers write
        modbus_register_array_t *values = ptk_alloc(allocator, sizeof(modbus_register_array_t), NULL);
        if(!values) {
            error("Failed to allocate register array");
            return PTK_ERR_NO_RESOURCES;
        }

        ptk_err array_err = modbus_register_array_create(allocator, values);
        if(array_err != PTK_OK) {
            error("Failed to create register array");
            ptk_free(allocator, values);
            return array_err;
        }

        for(size_t i = 0; i < config->write_values_count; i++) { modbus_register_array_append(values, config->write_values[i]); }

        err = client_send_write_holding_registers_req(conn, config->start_address, values);
        if(err != PTK_OK) {
            error("Failed to send write holding registers request");
            modbus_register_array_dispose(values);
            return err;
        }

        err = client_recv_write_holding_registers_resp(conn);
        if(err != PTK_OK) {
            error("Failed to receive write holding registers response");
            modbus_register_array_dispose(values);
            return err;
        }

        printf("Successfully wrote %zu values starting at holding register %d\n", config->write_values_count,
               config->start_address);

        modbus_register_array_dispose(values);
        ptk_free(allocator, values);
    }

    return PTK_OK;
}

ptk_err execute_write_coil(modbus_connection *conn, const client_config_t *config, ptk_allocator_t *allocator) {
    ptk_err err;

    if(config->write_values_count == 1) {
        // Single coil write
        err = client_send_write_coil_req(conn, config->start_address, config->write_coil_values[0]);
        if(err != PTK_OK) {
            error("Failed to send write coil request");
            return err;
        }

        err = client_recv_write_coil_resp(conn);
        if(err != PTK_OK) {
            error("Failed to receive write coil response");
            return err;
        }

        printf("Successfully wrote %s to coil %d\n", config->write_coil_values[0] ? "ON" : "OFF", config->start_address);
    } else {
        // Multiple coils write
        modbus_bool_array_t *values = ptk_alloc(allocator, sizeof(modbus_bool_array_t));
        if(!values) {
            error("Failed to allocate bool array");
            return PTK_ERR_NO_RESOURCES;
        }

        ptk_err array_err = modbus_bool_array_create(allocator, values);
        if(array_err != PTK_OK) {
            error("Failed to create bool array");
            ptk_free(allocator, values);
            return array_err;
        }

        for(size_t i = 0; i < config->write_values_count; i++) { modbus_bool_array_append(values, config->write_coil_values[i]); }

        err = client_send_write_coils_req(conn, config->start_address, values);
        if(err != PTK_OK) {
            error("Failed to send write coils request");
            modbus_bool_array_dispose(values);
            return err;
        }

        err = client_recv_write_coils_resp(conn);
        if(err != PTK_OK) {
            error("Failed to receive write coils response");
            modbus_bool_array_dispose(values);
            return err;
        }

        printf("Successfully wrote %zu values starting at coil %d\n", config->write_values_count, config->start_address);

        modbus_bool_array_dispose(values);
        ptk_free(allocator, values);
    }

    return PTK_OK;
}

//=============================================================================
// MAIN CLIENT LOGIC
//=============================================================================

int run_client(const client_config_t *config) {
    ptk_err err;

    // Create arena allocator for automatic cleanup
    ptk_allocator_t *allocator = allocator_arena_create(1024 * 1024, 8);  // 1MB arena, 8-byte alignment
    if(!allocator) {
        error("Failed to create arena allocator");
        return 1;
    }

    if(config->verbose) {
        info("Connecting to Modbus server at %s:%d (Unit ID: %d)", config->host, config->port, config->unit_id);
    }

    // Create address structure
    ptk_address_t server_addr;
    err = ptk_address_create(&server_addr, config->host, config->port);
    if(err != PTK_OK) {
        error("Failed to create server address");
        ptk_allocator_destroy(allocator);
        return 1;
    }

    // Create client buffer with automatic cleanup
    ptk_buf *client_buffer = ptk_buf_create(allocator, BUFFER_SIZE);
    if(!client_buffer) {
        error("Failed to create client buffer");
        ptk_allocator_destroy(allocator);
        return 1;
    }

    // Connect to Modbus server
    modbus_connection *conn = modbus_open_client(allocator, &server_addr, config->unit_id, client_buffer);
    if(!conn) {
        error("Failed to connect to Modbus server at %s:%d", config->host, config->port);
        ptk_allocator_destroy(allocator);
        return 1;
    }

    if(config->verbose) { info("Connected successfully. Executing operation..."); }

    // Execute the requested operation
    switch(config->operation) {
        case OP_READ_HOLDING: err = execute_read_holding_registers(conn, config); break;
        case OP_READ_INPUT: err = execute_read_input_registers(conn, config); break;
        case OP_READ_COILS: err = execute_read_coils(conn, config); break;
        case OP_READ_DISCRETE: err = execute_read_discrete_inputs(conn, config); break;
        case OP_WRITE_HOLDING:
        case OP_WRITE_HOLDING_MULTIPLE: err = execute_write_holding_register(conn, config, allocator); break;
        case OP_WRITE_COIL:
        case OP_WRITE_COILS_MULTIPLE: err = execute_write_coil(conn, config, allocator); break;
        default:
            error("Unknown operation");
            err = PTK_ERR_CONFIGURATION_ERROR;
            break;
    }

    // Close connection (not allocated from our allocator)
    modbus_close(conn);

    // Destroy arena allocator - this automatically cleans up ALL allocated resources:
    // - Client buffer
    // - Any temporary allocations made during operations
    if(config->verbose) { info("Cleaning up all resources..."); }
    ptk_allocator_destroy(allocator);

    if(err != PTK_OK) {
        error("Operation failed with error code: %d", err);
        return 1;
    }

    if(config->verbose) { info("Operation completed successfully"); }

    return 0;
}

//=============================================================================
// MAIN FUNCTION
//=============================================================================

int main(int argc, char *argv[]) {
    client_config_t config;

    // Parse command line arguments
    int parse_result = parse_arguments(argc, argv, &config);
    if(parse_result != 0) {
        return parse_result > 0 ? 0 : 1;  // Help message returns 0, errors return 1
    }

    // Run the client
    int result = run_client(&config);

    // Clean up allocated memory
    if(config.write_values) { free(config.write_values); }
    if(config.write_coil_values) { free(config.write_coil_values); }

    return result;
}
