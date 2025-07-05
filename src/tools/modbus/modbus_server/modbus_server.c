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

#include <stdlib.h>
#include <string.h>

#include "../protocol/modbus_defs.h"
#include "ptk_log.h"

typedef struct modbus_client_connection {
    ptk_sock *sock;
    char *remote_host;
    int remote_port;
    struct modbus_client_connection *next;
} modbus_client_connection_t;

struct modbus_server {
    ptk_ev_loop *loop;
    ptk_sock *server_sock;
    modbus_data_store_t *data_store;
    ptk_u8 unit_id;
    size_t max_connections;
    size_t active_connections;
    modbus_client_connection_t *clients;
    ptk_mutex *clients_mutex;
};

static modbus_client_connection_t* find_client_connection(modbus_server_t *server, ptk_sock *sock) {
    modbus_client_connection_t *client = server->clients;
    while (client) {
        if (client->sock == sock) {
            return client;
        }
        client = client->next;
    }
    return NULL;
}

static void add_client_connection(modbus_server_t *server, ptk_sock *sock, const char *remote_host, int remote_port) {
    ptk_mutex_wait_lock(server->clients_mutex, PTK_TIME_WAIT_FOREVER);
    
    modbus_client_connection_t *client = malloc(sizeof(modbus_client_connection_t));
    if (client) {
        client->sock = sock;
        client->remote_host = strdup(remote_host ? remote_host : "unknown");
        client->remote_port = remote_port;
        client->next = server->clients;
        server->clients = client;
        server->active_connections++;
        
        info("New Modbus client connected from %s:%d (total: %zu)",
             client->remote_host, client->remote_port, server->active_connections);
    }
    
    ptk_mutex_unlock(server->clients_mutex);
}

static void remove_client_connection(modbus_server_t *server, ptk_sock *sock) {
    ptk_mutex_wait_lock(server->clients_mutex, PTK_TIME_WAIT_FOREVER);
    
    modbus_client_connection_t **current = &server->clients;
    while (*current) {
        if ((*current)->sock == sock) {
            modbus_client_connection_t *to_remove = *current;
            *current = to_remove->next;
            
            info("Modbus client disconnected from %s:%d (total: %zu)",
                 to_remove->remote_host, to_remove->remote_port, server->active_connections - 1);
            
            free(to_remove->remote_host);
            free(to_remove);
            server->active_connections--;
            break;
        }
        current = &(*current)->next;
    }
    
    ptk_mutex_unlock(server->clients_mutex);
}

static void modbus_server_event_handler(const ptk_event *event) {
    modbus_server_t *server = (modbus_server_t*)event->user_data;
    
    switch (event->type) {
        case PTK_EVENT_ACCEPT: {
            if (server->active_connections >= server->max_connections) {
                warn("Maximum connections reached (%zu), rejecting new connection from %s:%d",
                     server->max_connections, event->remote_host, event->remote_port);
                ptk_tcp_close(event->sock);
                return;
            }
            
            add_client_connection(server, event->sock, event->remote_host, event->remote_port);
            break;
        }
        
        case PTK_EVENT_READ: {
            ptk_buf *request_buf = event->data;
            if (!request_buf) {
                error("Received read event with no data buffer");
                return;
            }
            
            // Get the amount of data available
            size_t data_size;
            ptk_err err = ptk_buf_get_cursor(&data_size, request_buf);
            if (err != PTK_OK || data_size < 7) { // Minimum MBAP header size
                debug("Insufficient data for Modbus request: %zu bytes", data_size);
                return; // Wait for more data
            }
            
            debug_buf("Received Modbus request", request_buf);
            
            // Process the Modbus request
            ptk_buf *response_buf = NULL;
            err = modbus_process_request(server->data_store, request_buf, &response_buf, server->unit_id);
            
            if (err == PTK_OK && response_buf) {
                debug_buf("Sending Modbus response", response_buf);
                
                // Send response back to client
                err = ptk_tcp_write(event->sock, response_buf);
                if (err != PTK_OK) {
                    error("Failed to send Modbus response: %s", ptk_err_to_string(err));
                    ptk_buf_free(response_buf);
                }
                // Note: ptk_tcp_write takes ownership of response_buf
            } else {
                if (response_buf) {
                    ptk_buf_free(response_buf);
                }
                error("Failed to process Modbus request: %s", ptk_err_to_string(err));
            }
            
            // Set cursor to indicate we consumed all the data
            ptk_buf_set_cursor(request_buf, data_size);
            break;
        }
        
        case PTK_EVENT_CLOSE: {
            remove_client_connection(server, event->sock);
            break;
        }
        
        case PTK_EVENT_ERROR: {
            modbus_client_connection_t *client = find_client_connection(server, event->sock);
            if (client) {
                error("Modbus client error from %s:%d: %s", 
                      client->remote_host, client->remote_port, ptk_err_to_string(event->error));
            } else {
                error("Modbus server error: %s", ptk_err_to_string(event->error));
            }
            
            remove_client_connection(server, event->sock);
            break;
        }
        
        default:
            debug("Unhandled event type: %d", event->type);
            break;
    }
}

modbus_err_t modbus_server_create(ptk_ev_loop *loop, modbus_server_t **server,
                                 const modbus_server_config_t *config) {
    if (!loop || !server || !config || !config->data_store) {
        return PTK_ERR_NULL_PTR;
    }
    
    modbus_server_t *new_server = calloc(1, sizeof(modbus_server_t));
    if (!new_server) {
        return PTK_ERR_NO_RESOURCES;
    }
    
    new_server->loop = loop;
    new_server->data_store = config->data_store;
    new_server->unit_id = config->unit_id;
    new_server->max_connections = config->max_connections ? config->max_connections : 10;
    new_server->active_connections = 0;
    new_server->clients = NULL;
    
    // Create mutex for client list
    ptk_err err = ptk_mutex_create(&new_server->clients_mutex, false);
    if (err != PTK_OK) {
        free(new_server);
        return err;
    }
    
    // Create TCP server socket
    ptk_tcp_server_opts opts = {
        .bind_host = config->bind_host,
        .bind_port = config->bind_port ? config->bind_port : MODBUS_TCP_PORT,
        .callback = modbus_server_event_handler,
        .user_data = new_server,
        .backlog = (int)new_server->max_connections
    };
    
    err = ptk_tcp_server_create(loop, &new_server->server_sock, &opts);
    if (err != PTK_OK) {
        ptk_mutex_destroy(new_server->clients_mutex);
        free(new_server);
        return err;
    }
    
    *server = new_server;
    
    info("Modbus TCP server created on %s:%d (unit ID: %d, max connections: %zu)",
         opts.bind_host ? opts.bind_host : "0.0.0.0", opts.bind_port,
         new_server->unit_id, new_server->max_connections);
    
    return PTK_OK;
}

void modbus_server_destroy(modbus_server_t *server) {
    if (!server) {
        return;
    }
    
    // Close server socket
    if (server->server_sock) {
        ptk_tcp_close(server->server_sock);
    }
    
    // Clean up client connections
    if (server->clients_mutex) {
        ptk_mutex_wait_lock(server->clients_mutex, PTK_TIME_WAIT_FOREVER);
        
        modbus_client_connection_t *client = server->clients;
        while (client) {
            modbus_client_connection_t *next = client->next;
            
            if (client->sock) {
                ptk_tcp_close(client->sock);
            }
            free(client->remote_host);
            free(client);
            
            client = next;
        }
        
        ptk_mutex_unlock(server->clients_mutex);
        ptk_mutex_destroy(server->clients_mutex);
    }
    
    free(server);
    
    info("Modbus TCP server destroyed");
}