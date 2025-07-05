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
#include <signal.h>
#include <string.h>

#include "../protocol/modbus_defs.h"
#include "ptk_log.h"
#include "ptk_utils.h"

static ptk_ev_loop *g_loop = NULL;
static modbus_server_t *g_server = NULL;

static void signal_handler(int sig) {
    info("Received signal %d, shutting down...", sig);
    
    if (g_server) {
        modbus_server_destroy(g_server);
        g_server = NULL;
    }
    
    if (g_loop) {
        ptk_loop_stop(g_loop);
    }
}

static void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("\nOptions:\n");
    printf("  -h, --help              Show this help message\n");
    printf("  -p, --port PORT         Bind to PORT (default: 502)\n");
    printf("  -b, --bind HOST         Bind to HOST (default: all interfaces)\n");
    printf("  -u, --unit-id ID        Set unit identifier (default: 1)\n");
    printf("  -c, --max-connections N Maximum concurrent connections (default: 10)\n");
    printf("  -v, --verbose           Enable verbose logging\n");
    printf("  --debug                 Enable debug logging\n");
    printf("\nExample:\n");
    printf("  %s --port 5020 --unit-id 5 --verbose\n", program_name);
}

int main(int argc, char *argv[]) {
    // Default configuration
    const char *bind_host = NULL;
    int bind_port = MODBUS_TCP_PORT;
    ptk_u8 unit_id = 1;
    size_t max_connections = 10;
    ptk_log_level log_level = PTK_LOG_LEVEL_INFO;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: %s requires a port number\n", argv[i-1]);
                return 1;
            }
            bind_port = atoi(argv[i]);
            if (bind_port <= 0 || bind_port > 65535) {
                fprintf(stderr, "Error: Invalid port number: %s\n", argv[i]);
                return 1;
            }
        } else if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--bind") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: %s requires a host address\n", argv[i-1]);
                return 1;
            }
            bind_host = argv[i];
        } else if (strcmp(argv[i], "-u") == 0 || strcmp(argv[i], "--unit-id") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: %s requires a unit ID\n", argv[i-1]);
                return 1;
            }
            int id = atoi(argv[i]);
            if (id < 0 || id > 255) {
                fprintf(stderr, "Error: Invalid unit ID: %s (must be 0-255)\n", argv[i]);
                return 1;
            }
            unit_id = (ptk_u8)id;
        } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--max-connections") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: %s requires a connection count\n", argv[i-1]);
                return 1;
            }
            max_connections = (size_t)atoi(argv[i]);
            if (max_connections == 0) {
                fprintf(stderr, "Error: Invalid max connections: %s\n", argv[i]);
                return 1;
            }
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            log_level = PTK_LOG_LEVEL_DEBUG;
        } else if (strcmp(argv[i], "--debug") == 0) {
            log_level = PTK_LOG_LEVEL_TRACE;
        } else {
            fprintf(stderr, "Error: Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }
    
    // Set log level
    ptk_log_level_set(log_level);
    
    // Set up signal handlers
    ptk_set_interrupt_handler(signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    
    info("Starting Modbus TCP server...");
    info("  Bind address: %s", bind_host ? bind_host : "all interfaces");
    info("  Port: %d", bind_port);
    info("  Unit ID: %d", unit_id);
    info("  Max connections: %zu", max_connections);
    
    // Create event loop
    ptk_loop_config loop_config = {
        .worker_threads = 2,
        .worker_thread_stack_size = 0 // Use default
    };
    
    ptk_err err = ptk_loop_create(&g_loop, &loop_config);
    if (err != PTK_OK) {
        error("Failed to create event loop: %s", ptk_err_to_string(err));
        return 1;
    }
    
    // Create data store with default configuration
    modbus_data_store_t *data_store;
    err = modbus_data_store_create(&data_store, NULL);
    if (err != PTK_OK) {
        error("Failed to create data store: %s", ptk_err_to_string(err));
        ptk_loop_destroy(g_loop);
        return 1;
    }
    
    // Initialize some test data
    ptk_u16_be test_registers[] = {100, 200, 300, 400, 500};
    modbus_data_store_write_holding_registers(data_store, 0, 5, test_registers);
    
    ptk_u8 test_coils[] = {1, 0, 1, 1, 0, 1, 0, 1};
    modbus_data_store_write_coils(data_store, 0, 8, test_coils);
    
    info("Initialized test data: 5 holding registers and 8 coils");
    
    // Create server
    modbus_server_config_t server_config = {
        .bind_host = bind_host,
        .bind_port = bind_port,
        .data_store = data_store,
        .unit_id = unit_id,
        .max_connections = max_connections
    };
    
    err = modbus_server_create(g_loop, &g_server, &server_config);
    if (err != PTK_OK) {
        error("Failed to create Modbus server: %s", ptk_err_to_string(err));
        modbus_data_store_destroy(data_store);
        ptk_loop_destroy(g_loop);
        return 1;
    }
    
    info("Modbus TCP server started successfully");
    info("Press Ctrl+C to stop the server");
    
    // Run the event loop
    err = ptk_loop_wait(g_loop);
    if (err != PTK_OK) {
        error("Event loop error: %s", ptk_err_to_string(err));
    }
    
    // Cleanup
    info("Shutting down...");
    
    if (g_server) {
        modbus_server_destroy(g_server);
    }
    
    modbus_data_store_destroy(data_store);
    ptk_loop_destroy(g_loop);
    
    info("Modbus TCP server stopped");
    return 0;
}