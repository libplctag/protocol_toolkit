/***************************************************************************
 *   Copyright (C) 2025 by Kyle Hayes                                      *
 *   Author Kyle Hayes  kyle.hayes@gmail.com                               *
 *                                                                         *
 * This software is available under either the Mozilla Public License      *
 * version 2.0 or the GNU LGPL version 2 (or later) license, whichever     *
 * you choose.                                                             *
 *                                                                         *
 * MPL 2.0:                                                                *
 *                                                                         *
 * This Source Code Form is subject to the terms of the Mozilla Public    *
 * License, v. 2.0. If a copy of the MPL was not distributed with this    *
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.               *
 *                                                                         *
 * LGPL 2:                                                                 *
 *                                                                         *
 * This program is free software; you can redistribute it and/or modify    *
 * it under the terms of the GNU Library General Public License as        *
 * published by the Free Software Foundation; either version 2 of the     *
 * License, or (at your option) any later version.                        *
 *                                                                         *
 * This program is distributed in the hope that it will be useful,        *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 * GNU General Public License for more details.                           *
 *                                                                         *
 * You should have received a copy of the GNU Library General Public      *
 * License along with this program; if not, write to the Free Software    *
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307   *
 * USA                                                                     *
 *                                                                         *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../protocol/modbus_defs.h"
#include "ptk_log.h"
#include "ptk_utils.h"

static void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS] HOST COMMAND [ARGS...]\n", program_name);
    printf("\nOptions:\n");
    printf("  -h, --help              Show this help message\n");
    printf("  -p, --port PORT         Connect to PORT (default: 502)\n");
    printf("  -u, --unit-id ID        Set unit identifier (default: 1)\n");
    printf("  -t, --timeout MS        Set timeout in milliseconds (default: 5000)\n");
    printf("  -v, --verbose           Enable verbose logging\n");
    printf("  --debug                 Enable debug logging\n");
    printf("\nCommands:\n");
    printf("  read-coils ADDR COUNT           Read COUNT coils starting at ADDR\n");
    printf("  read-holding ADDR COUNT         Read COUNT holding registers starting at ADDR\n");
    printf("  write-coil ADDR VALUE           Write VALUE (0/1) to coil at ADDR\n");
    printf("  write-register ADDR VALUE       Write VALUE to holding register at ADDR\n");
    printf("\nExamples:\n");
    printf("  %s 192.168.1.100 read-holding 0 10\n", program_name);
    printf("  %s localhost write-coil 5 1\n", program_name);
    printf("  %s -p 5020 -u 5 192.168.1.100 read-coils 0 8\n", program_name);
}

static ptk_err run_read_coils(modbus_client_t *client, int argc, char *argv[]) {
    if(argc < 2) {
        fprintf(stderr, "Error: read-coils requires address and count\n");
        return PTK_ERR_INVALID_PARAM;
    }

    ptk_u16_be address = (ptk_u16_be)atoi(argv[0]);
    ptk_u16_be count = (ptk_u16_be)atoi(argv[1]);

    if(count == 0 || count > MODBUS_MAX_COILS) {
        fprintf(stderr, "Error: Invalid coil count: %d (must be 1-%d)\n", count, MODBUS_MAX_COILS);
        return PTK_ERR_INVALID_PARAM;
    }

    size_t byte_count = (count + 7) / 8;
    ptk_u8 *coil_data = malloc(byte_count);
    if(!coil_data) { return PTK_ERR_NO_RESOURCES; }

    printf("Reading %d coils starting at address %d...\n", count, address);

    ptk_err err = modbus_client_read_coils(client, address, count, coil_data);
    if(err != PTK_OK) {
        fprintf(stderr, "Error reading coils: %s\n", ptk_err_to_string(err));
        free(coil_data);
        return err;
    }

    printf("Coil values:\n");
    for(ptk_u16_be i = 0; i < count; i++) {
        ptk_u16_be byte_idx = i / 8;
        ptk_u8 bit_idx = i % 8;
        int value = (coil_data[byte_idx] & (1 << bit_idx)) ? 1 : 0;
        printf("  Coil %d: %d\n", address + i, value);
    }

    free(coil_data);
    return PTK_OK;
}

static ptk_err run_read_holding_registers(modbus_client_t *client, int argc, char *argv[]) {
    if(argc < 2) {
        fprintf(stderr, "Error: read-holding requires address and count\n");
        return PTK_ERR_INVALID_PARAM;
    }

    ptk_u16_be address = (ptk_u16_be)atoi(argv[0]);
    ptk_u16_be count = (ptk_u16_be)atoi(argv[1]);

    if(count == 0 || count > MODBUS_MAX_REGISTERS) {
        fprintf(stderr, "Error: Invalid register count: %d (must be 1-%d)\n", count, MODBUS_MAX_REGISTERS);
        return PTK_ERR_INVALID_PARAM;
    }

    ptk_u16_be *register_data = malloc(count * sizeof(ptk_u16_be));
    if(!register_data) { return PTK_ERR_NO_RESOURCES; }

    printf("Reading %d holding registers starting at address %d...\n", count, address);

    ptk_err err = modbus_client_read_holding_registers(client, address, count, register_data);
    if(err != PTK_OK) {
        fprintf(stderr, "Error reading holding registers: %s\n", ptk_err_to_string(err));
        free(register_data);
        return err;
    }

    printf("Register values:\n");
    for(ptk_u16_be i = 0; i < count; i++) { printf("  Register %d: %d\n", address + i, register_data[i]); }

    free(register_data);
    return PTK_OK;
}

static ptk_err run_write_coil(modbus_client_t *client, int argc, char *argv[]) {
    if(argc < 2) {
        fprintf(stderr, "Error: write-coil requires address and value\n");
        return PTK_ERR_INVALID_PARAM;
    }

    ptk_u16_be address = (ptk_u16_be)atoi(argv[0]);
    int value = atoi(argv[1]);

    if(value != 0 && value != 1) {
        fprintf(stderr, "Error: Invalid coil value: %d (must be 0 or 1)\n", value);
        return PTK_ERR_INVALID_PARAM;
    }

    printf("Writing coil %d = %d...\n", address, value);

    ptk_err err = modbus_client_write_single_coil(client, address, value != 0);
    if(err != PTK_OK) {
        fprintf(stderr, "Error writing coil: %s\n", ptk_err_to_string(err));
        return err;
    }

    printf("Successfully wrote coil %d = %d\n", address, value);
    return PTK_OK;
}

static ptk_err run_write_register(modbus_client_t *client, int argc, char *argv[]) {
    if(argc < 2) {
        fprintf(stderr, "Error: write-register requires address and value\n");
        return PTK_ERR_INVALID_PARAM;
    }

    ptk_u16_be address = (ptk_u16_be)atoi(argv[0]);
    ptk_u16_be value = (ptk_u16_be)atoi(argv[1]);

    printf("Writing register %d = %d...\n", address, value);

    ptk_err err = modbus_client_write_single_register(client, address, value);
    if(err != PTK_OK) {
        fprintf(stderr, "Error writing register: %s\n", ptk_err_to_string(err));
        return err;
    }

    printf("Successfully wrote register %d = %d\n", address, value);
    return PTK_OK;
}

int main(int argc, char *argv[]) {
    // Default configuration
    const char *host = NULL;
    int port = MODBUS_TCP_PORT;
    ptk_u8 unit_id = 1;
    uint32_t timeout_ms = 5000;
    ptk_log_level log_level = PTK_LOG_LEVEL_WARN;
    const char *command = NULL;
    char **command_args = NULL;
    int command_argc = 0;

    // Parse command line arguments
    int i;
    for(i = 1; i < argc; i++) {
        if(strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if(strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) {
            if(++i >= argc) {
                fprintf(stderr, "Error: %s requires a port number\n", argv[i - 1]);
                return 1;
            }
            port = atoi(argv[i]);
            if(port <= 0 || port > 65535) {
                fprintf(stderr, "Error: Invalid port number: %s\n", argv[i]);
                return 1;
            }
        } else if(strcmp(argv[i], "-u") == 0 || strcmp(argv[i], "--unit-id") == 0) {
            if(++i >= argc) {
                fprintf(stderr, "Error: %s requires a unit ID\n", argv[i - 1]);
                return 1;
            }
            int id = atoi(argv[i]);
            if(id < 0 || id > 255) {
                fprintf(stderr, "Error: Invalid unit ID: %s (must be 0-255)\n", argv[i]);
                return 1;
            }
            unit_id = (ptk_u8)id;
        } else if(strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--timeout") == 0) {
            if(++i >= argc) {
                fprintf(stderr, "Error: %s requires a timeout value\n", argv[i - 1]);
                return 1;
            }
            timeout_ms = (uint32_t)atoi(argv[i]);
            if(timeout_ms == 0) {
                fprintf(stderr, "Error: Invalid timeout: %s\n", argv[i]);
                return 1;
            }
        } else if(strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            log_level = PTK_LOG_LEVEL_INFO;
        } else if(strcmp(argv[i], "--debug") == 0) {
            log_level = PTK_LOG_LEVEL_DEBUG;
        } else if(argv[i][0] == '-') {
            fprintf(stderr, "Error: Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        } else {
            // First non-option argument should be the host
            if(!host) {
                host = argv[i];
            } else if(!command) {
                command = argv[i];
                command_args = &argv[i + 1];
                command_argc = argc - i - 1;
                break;
            }
        }
    }

    if(!host || !command) {
        fprintf(stderr, "Error: Missing required arguments\n");
        print_usage(argv[0]);
        return 1;
    }

    // Set log level
    ptk_log_level_set(log_level);

    info("Connecting to Modbus server %s:%d (unit ID: %d)", host, port, unit_id);

    // Create event loop
    ptk_loop_config loop_config = {
        .worker_threads = 1,
        .worker_thread_stack_size = 0  // Use default
    };

    ptk_ev_loop *loop;
    ptk_err err = ptk_loop_create(&loop, &loop_config);
    if(err != PTK_OK) {
        error("Failed to create event loop: %s", ptk_err_to_string(err));
        return 1;
    }

    // Create client
    modbus_client_config_t client_config = {.host = host, .port = port, .unit_id = unit_id, .timeout_ms = timeout_ms};

    modbus_client_t *client;
    err = modbus_client_create(loop, &client, &client_config);
    if(err != PTK_OK) {
        error("Failed to create Modbus client: %s", ptk_err_to_string(err));
        ptk_loop_destroy(loop);
        return 1;
    }

    // Execute command
    if(strcmp(command, "read-coils") == 0) {
        err = run_read_coils(client, command_argc, command_args);
    } else if(strcmp(command, "read-holding") == 0) {
        err = run_read_holding_registers(client, command_argc, command_args);
    } else if(strcmp(command, "write-coil") == 0) {
        err = run_write_coil(client, command_argc, command_args);
    } else if(strcmp(command, "write-register") == 0) {
        err = run_write_register(client, command_argc, command_args);
    } else {
        fprintf(stderr, "Error: Unknown command: %s\n", command);
        print_usage(argv[0]);
        err = PTK_ERR_INVALID_PARAM;
    }

    // Cleanup
    modbus_client_destroy(client);
    ptk_loop_destroy(loop);

    if(err == PTK_OK) {
        info("Command completed successfully");
        return 0;
    } else {
        return 1;
    }
}
