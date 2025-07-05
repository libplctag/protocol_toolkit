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
#include "ptk_atomic.h"
#include "ptk_log.h"

typedef enum {
    MODBUS_CLIENT_STATE_DISCONNECTED,
    MODBUS_CLIENT_STATE_CONNECTING,
    MODBUS_CLIENT_STATE_CONNECTED,
    MODBUS_CLIENT_STATE_ERROR
} modbus_client_state_t;

typedef struct modbus_request {
    ptk_u16_be transaction_id;
    ptk_buf *response_buf;
    ptk_mutex *response_mutex;
    ptk_cond_var *response_cond;
    ptk_err response_error;
    bool response_received;
    uint32_t timeout_ms;
    uint64_t sent_time_ms;
} modbus_request_t;

struct modbus_client {
    ptk_ev_loop *loop;
    ptk_sock *sock;
    char *host;
    int port;
    ptk_u8 unit_id;
    uint32_t timeout_ms;

    modbus_client_state_t state;
    ptk_mutex *state_mutex;

    ptk_u16_be next_transaction_id;
    modbus_request_t pending_request;
    bool request_active;
    ptk_mutex *request_mutex;
};

static void modbus_client_event_handler(const ptk_event *event);

modbus_err_t modbus_client_create(ptk_ev_loop *loop, modbus_client_t **client, const modbus_client_config_t *config) {
    if(!loop || !client || !config || !config->host) { return PTK_ERR_NULL_PTR; }

    modbus_client_t *new_client = calloc(1, sizeof(modbus_client_t));
    if(!new_client) { return PTK_ERR_NO_RESOURCES; }

    new_client->loop = loop;
    new_client->host = strdup(config->host);
    new_client->port = config->port ? config->port : MODBUS_TCP_PORT;
    new_client->unit_id = config->unit_id;
    new_client->timeout_ms = config->timeout_ms ? config->timeout_ms : 5000;
    new_client->state = MODBUS_CLIENT_STATE_DISCONNECTED;
    new_client->next_transaction_id = 1;
    new_client->request_active = false;

    if(!new_client->host) {
        free(new_client);
        return PTK_ERR_NO_RESOURCES;
    }

    // Create mutexes
    ptk_err err = ptk_mutex_create(&new_client->state_mutex, false);
    if(err != PTK_OK) {
        free(new_client->host);
        free(new_client);
        return err;
    }

    err = ptk_mutex_create(&new_client->request_mutex, false);
    if(err != PTK_OK) {
        ptk_mutex_destroy(new_client->state_mutex);
        free(new_client->host);
        free(new_client);
        return err;
    }

    *client = new_client;

    info("Modbus TCP client created for %s:%d (unit ID: %d)", new_client->host, new_client->port, new_client->unit_id);

    return PTK_OK;
}

void modbus_client_destroy(modbus_client_t *client) {
    if(!client) { return; }

    // Close connection if open
    if(client->sock) { ptk_tcp_close(client->sock); }

    // Clean up pending request
    if(client->request_mutex) {
        ptk_mutex_wait_lock(client->request_mutex, PTK_TIME_WAIT_FOREVER);

        if(client->request_active && client->pending_request.response_mutex) {
            ptk_mutex_destroy(client->pending_request.response_mutex);
        }
        if(client->request_active && client->pending_request.response_cond) {
            ptk_cond_var_destroy(client->pending_request.response_cond);
        }
        if(client->request_active && client->pending_request.response_buf) { ptk_buf_free(client->pending_request.response_buf); }

        ptk_mutex_unlock(client->request_mutex);
        ptk_mutex_destroy(client->request_mutex);
    }

    if(client->state_mutex) { ptk_mutex_destroy(client->state_mutex); }

    free(client->host);
    free(client);

    info("Modbus TCP client destroyed");
}

static ptk_err modbus_client_connect(modbus_client_t *client) {
    ptk_mutex_wait_lock(client->state_mutex, PTK_TIME_WAIT_FOREVER);

    if(client->state == MODBUS_CLIENT_STATE_CONNECTED) {
        ptk_mutex_unlock(client->state_mutex);
        return PTK_OK;
    }

    if(client->state == MODBUS_CLIENT_STATE_CONNECTING) {
        ptk_mutex_unlock(client->state_mutex);
        return PTK_ERR_WOULD_BLOCK;
    }

    client->state = MODBUS_CLIENT_STATE_CONNECTING;
    ptk_mutex_unlock(client->state_mutex);

    // Create TCP connection
    ptk_tcp_client_opts opts = {.host = client->host,
                                .port = client->port,
                                .callback = modbus_client_event_handler,
                                .user_data = client,
                                .connect_timeout_ms = client->timeout_ms,
                                .keep_alive = true};

    ptk_err err = ptk_tcp_client_create(client->loop, &client->sock, &opts);
    if(err != PTK_OK) {
        ptk_mutex_wait_lock(client->state_mutex, PTK_TIME_WAIT_FOREVER);
        client->state = MODBUS_CLIENT_STATE_ERROR;
        ptk_mutex_unlock(client->state_mutex);
        return err;
    }

    return PTK_OK;
}

static void modbus_client_event_handler(const ptk_event *event) {
    modbus_client_t *client = (modbus_client_t *)event->user_data;

    switch(event->type) {
        case PTK_EVENT_CONNECT: {
            ptk_mutex_wait_lock(client->state_mutex, PTK_TIME_WAIT_FOREVER);

            if(event->error == PTK_OK) {
                client->state = MODBUS_CLIENT_STATE_CONNECTED;
                info("Modbus client connected to %s:%d", client->host, client->port);
            } else {
                client->state = MODBUS_CLIENT_STATE_ERROR;
                error("Failed to connect to Modbus server %s:%d: %s", client->host, client->port,
                      ptk_err_to_string(event->error));
            }

            ptk_mutex_unlock(client->state_mutex);
            break;
        }

        case PTK_EVENT_READ: {
            ptk_buf *response_buf = event->data;
            if(!response_buf) {
                error("Received read event with no data buffer");
                return;
            }

            // Get the amount of data available
            size_t data_size;
            ptk_err err = ptk_buf_get_cursor(&data_size, response_buf);
            if(err != PTK_OK || data_size < 7) {  // Minimum MBAP header size
                debug("Insufficient data for Modbus response: %zu bytes", data_size);
                return;  // Wait for more data
            }

            debug_buf("Received Modbus response", response_buf);

            ptk_mutex_wait_lock(client->request_mutex, PTK_TIME_WAIT_FOREVER);

            if(client->request_active) {
                // Reset cursor to parse header
                ptk_buf_set_cursor(response_buf, 0);

                // Decode MBAP header to get transaction ID
                modbus_mbap_header_t mbap;
                err = modbus_mbap_header_decode(&mbap, response_buf);

                if(err == PTK_OK && mbap.transaction_id == client->pending_request.transaction_id) {
                    // This is our response
                    client->pending_request.response_buf = response_buf;
                    client->pending_request.response_error = PTK_OK;
                    client->pending_request.response_received = true;

                    ptk_mutex_wait_lock(client->pending_request.response_mutex, PTK_TIME_WAIT_FOREVER);
                    ptk_cond_var_signal(client->pending_request.response_cond);
                    ptk_mutex_unlock(client->pending_request.response_mutex);

                    // Don't set cursor since we want to keep the response data
                } else {
                    warn("Received unexpected Modbus response (transaction ID: %d, expected: %d)", mbap.transaction_id,
                         client->pending_request.transaction_id);
                    ptk_buf_set_cursor(response_buf, data_size);  // Consume the data
                }
            } else {
                warn("Received unexpected Modbus response with no pending request");
                ptk_buf_set_cursor(response_buf, data_size);  // Consume the data
            }

            ptk_mutex_unlock(client->request_mutex);
            break;
        }

        case PTK_EVENT_CLOSE: {
            ptk_mutex_wait_lock(client->state_mutex, PTK_TIME_WAIT_FOREVER);
            client->state = MODBUS_CLIENT_STATE_DISCONNECTED;
            client->sock = NULL;
            ptk_mutex_unlock(client->state_mutex);

            info("Modbus client disconnected from %s:%d", client->host, client->port);
            break;
        }

        case PTK_EVENT_ERROR: {
            ptk_mutex_wait_lock(client->state_mutex, PTK_TIME_WAIT_FOREVER);
            client->state = MODBUS_CLIENT_STATE_ERROR;
            ptk_mutex_unlock(client->state_mutex);

            error("Modbus client error: %s", ptk_err_to_string(event->error));
            break;
        }

        default: debug("Unhandled event type: %d", event->type); break;
    }
}

static modbus_err_t modbus_client_send_request(modbus_client_t *client, ptk_buf *request_buf, ptk_buf **response_buf) {
    if(!client || !request_buf || !response_buf) { return PTK_ERR_NULL_PTR; }

    *response_buf = NULL;

    // Ensure we're connected
    ptk_err err = modbus_client_connect(client);
    if(err != PTK_OK) { return err; }

    // Wait for connection to be established
    uint32_t wait_time = 0;
    const uint32_t wait_increment = 10;  // ms

    while(wait_time < client->timeout_ms) {
        ptk_mutex_wait_lock(client->state_mutex, PTK_TIME_WAIT_FOREVER);
        modbus_client_state_t state = client->state;
        ptk_mutex_unlock(client->state_mutex);

        if(state == MODBUS_CLIENT_STATE_CONNECTED) { break; }
        if(state == MODBUS_CLIENT_STATE_ERROR) { return PTK_ERR_CONNECTION_REFUSED; }

        ptk_thread_sleep_ms(wait_increment);
        wait_time += wait_increment;
    }

    if(wait_time >= client->timeout_ms) { return PTK_ERR_TIMEOUT; }

    ptk_mutex_wait_lock(client->request_mutex, PTK_TIME_WAIT_FOREVER);

    if(client->request_active) {
        ptk_mutex_unlock(client->request_mutex);
        return PTK_ERR_DEVICE_BUSY;
    }

    // Set up request tracking
    client->request_active = true;
    client->pending_request.transaction_id = client->next_transaction_id++;
    client->pending_request.response_buf = NULL;
    client->pending_request.response_received = false;
    client->pending_request.response_error = PTK_OK;
    client->pending_request.timeout_ms = client->timeout_ms;
    client->pending_request.sent_time_ms = ptk_now_ms();

    err = ptk_mutex_create(&client->pending_request.response_mutex, false);
    if(err != PTK_OK) {
        client->request_active = false;
        ptk_mutex_unlock(client->request_mutex);
        return err;
    }

    err = ptk_cond_var_create(&client->pending_request.response_cond);
    if(err != PTK_OK) {
        ptk_mutex_destroy(client->pending_request.response_mutex);
        client->request_active = false;
        ptk_mutex_unlock(client->request_mutex);
        return err;
    }

    // Update transaction ID in request buffer
    ptk_buf_set_cursor(request_buf, 0);
    err = ptk_buf_encode(request_buf, "ptk_u16_be", client->pending_request.transaction_id);
    if(err != PTK_OK) {
        ptk_cond_var_destroy(client->pending_request.response_cond);
        ptk_mutex_destroy(client->pending_request.response_mutex);
        client->request_active = false;
        ptk_mutex_unlock(client->request_mutex);
        return err;
    }

    ptk_mutex_unlock(client->request_mutex);

    // Send request
    debug_buf("Sending Modbus request", request_buf);
    err = ptk_tcp_write(client->sock, request_buf);
    if(err != PTK_OK) {
        ptk_mutex_wait_lock(client->request_mutex, PTK_TIME_WAIT_FOREVER);
        ptk_cond_var_destroy(client->pending_request.response_cond);
        ptk_mutex_destroy(client->pending_request.response_mutex);
        client->request_active = false;
        ptk_mutex_unlock(client->request_mutex);
        return err;
    }

    // Wait for response
    ptk_mutex_wait_lock(client->pending_request.response_mutex, PTK_TIME_WAIT_FOREVER);

    while(!client->pending_request.response_received) {
        err =
            ptk_cond_var_wait(client->pending_request.response_cond, client->pending_request.response_mutex, client->timeout_ms);
        if(err == PTK_ERR_TIMEOUT) { break; }
    }

    *response_buf = client->pending_request.response_buf;
    ptk_err response_error = client->pending_request.response_error;

    ptk_mutex_unlock(client->pending_request.response_mutex);

    // Clean up request tracking
    ptk_mutex_wait_lock(client->request_mutex, PTK_TIME_WAIT_FOREVER);
    ptk_cond_var_destroy(client->pending_request.response_cond);
    ptk_mutex_destroy(client->pending_request.response_mutex);
    client->request_active = false;
    ptk_mutex_unlock(client->request_mutex);

    if(err == PTK_ERR_TIMEOUT) { return PTK_ERR_TIMEOUT; }

    return response_error;
}

modbus_err_t modbus_client_read_coils(modbus_client_t *client, ptk_u16_be address, ptk_u16_be count, ptk_u8 *values) {
    if(!client || !values) { return PTK_ERR_NULL_PTR; }

    if(count == 0 || count > MODBUS_MAX_COILS) { return PTK_ERR_INVALID_PARAM; }

    // Create request buffer
    ptk_buf *request_buf;
    ptk_err err = ptk_buf_alloc(&request_buf, 7 + 5);  // MBAP + FC + address + count
    if(err != PTK_OK) { return err; }

    // Create MBAP header
    modbus_mbap_header_t mbap = {.transaction_id = 0,  // Will be filled by send_request
                                 .protocol_id = 0,
                                 .length = 5,  // Unit ID + FC + address + count
                                 .unit_id = client->unit_id};

    err = modbus_mbap_header_encode(request_buf, &mbap);
    if(err == PTK_OK) { err = ptk_buf_encode(request_buf, "ptk_u8 ptk_u16_be ptk_u16_be", MODBUS_FC_READ_COILS, address, count); }

    if(err != PTK_OK) {
        ptk_buf_free(request_buf);
        return err;
    }

    // Send request and get response
    ptk_buf *response_buf;
    err = modbus_client_send_request(client, request_buf, &response_buf);
    if(err != PTK_OK) { return err; }

    if(!response_buf) { return PTK_ERR_PROTOCOL_ERROR; }

    // Parse response
    ptk_buf_set_cursor(response_buf, 7);  // Skip MBAP header

    ptk_u8 function_code, byte_count;
    err = ptk_buf_decode(response_buf, "ptk_u8 ptk_u8", &function_code, &byte_count);

    if(err != PTK_OK) {
        ptk_buf_free(response_buf);
        return PTK_ERR_PARSE_ERROR;
    }

    if(function_code & 0x80) {
        // Exception response
        ptk_u8 exception_code = byte_count;  // Second byte is exception code
        ptk_buf_free(response_buf);

        switch(exception_code) {
            case MODBUS_EX_ILLEGAL_DATA_ADDRESS: return PTK_ERR_OUT_OF_BOUNDS;
            case MODBUS_EX_ILLEGAL_FUNCTION: return PTK_ERR_UNSUPPORTED_VERSION;
            default: return PTK_ERR_DEVICE_FAILURE;
        }
    }

    if(function_code != MODBUS_FC_READ_COILS) {
        ptk_buf_free(response_buf);
        return PTK_ERR_PROTOCOL_ERROR;
    }

    size_t expected_bytes = (count + 7) / 8;
    if(byte_count != expected_bytes) {
        ptk_buf_free(response_buf);
        return PTK_ERR_PROTOCOL_ERROR;
    }

    // Read coil data
    for(int i = 0; i < byte_count; i++) {
        err = ptk_buf_decode(response_buf, "ptk_u8", &values[i]);
        if(err != PTK_OK) {
            ptk_buf_free(response_buf);
            return PTK_ERR_PARSE_ERROR;
        }
    }

    ptk_buf_free(response_buf);
    return PTK_OK;
}

modbus_err_t modbus_client_read_holding_registers(modbus_client_t *client, ptk_u16_be address, ptk_u16_be count,
                                                  ptk_u16_be *values) {
    if(!client || !values) { return PTK_ERR_NULL_PTR; }

    if(count == 0 || count > MODBUS_MAX_REGISTERS) { return PTK_ERR_INVALID_PARAM; }

    // Create request buffer
    ptk_buf *request_buf;
    ptk_err err = ptk_buf_alloc(&request_buf, 7 + 5);  // MBAP + FC + address + count
    if(err != PTK_OK) { return err; }

    // Create MBAP header
    modbus_mbap_header_t mbap = {.transaction_id = 0,  // Will be filled by send_request
                                 .protocol_id = 0,
                                 .length = 5,  // Unit ID + FC + address + count
                                 .unit_id = client->unit_id};

    err = modbus_mbap_header_encode(request_buf, &mbap);
    if(err == PTK_OK) {
        err = ptk_buf_encode(request_buf, "ptk_u8 ptk_u16_be ptk_u16_be", MODBUS_FC_READ_HOLDING_REGISTERS, address, count);
    }

    if(err != PTK_OK) {
        ptk_buf_free(request_buf);
        return err;
    }

    // Send request and get response
    ptk_buf *response_buf;
    err = modbus_client_send_request(client, request_buf, &response_buf);
    if(err != PTK_OK) { return err; }

    if(!response_buf) { return PTK_ERR_PROTOCOL_ERROR; }

    // Parse response
    ptk_buf_set_cursor(response_buf, 7);  // Skip MBAP header

    ptk_u8 function_code, byte_count;
    err = ptk_buf_decode(response_buf, "ptk_u8 ptk_u8", &function_code, &byte_count);

    if(err != PTK_OK) {
        ptk_buf_free(response_buf);
        return PTK_ERR_PARSE_ERROR;
    }

    if(function_code & 0x80) {
        // Exception response
        ptk_u8 exception_code = byte_count;  // Second byte is exception code
        ptk_buf_free(response_buf);

        switch(exception_code) {
            case MODBUS_EX_ILLEGAL_DATA_ADDRESS: return PTK_ERR_OUT_OF_BOUNDS;
            case MODBUS_EX_ILLEGAL_FUNCTION: return PTK_ERR_UNSUPPORTED_VERSION;
            default: return PTK_ERR_DEVICE_FAILURE;
        }
    }

    if(function_code != MODBUS_FC_READ_HOLDING_REGISTERS) {
        ptk_buf_free(response_buf);
        return PTK_ERR_PROTOCOL_ERROR;
    }

    if(byte_count != count * 2) {
        ptk_buf_free(response_buf);
        return PTK_ERR_PROTOCOL_ERROR;
    }

    // Read register data
    for(int i = 0; i < count; i++) {
        err = ptk_buf_decode(response_buf, "ptk_u16_be", &values[i]);
        if(err != PTK_OK) {
            ptk_buf_free(response_buf);
            return PTK_ERR_PARSE_ERROR;
        }
    }

    ptk_buf_free(response_buf);
    return PTK_OK;
}

modbus_err_t modbus_client_write_single_coil(modbus_client_t *client, ptk_u16_be address, bool value) {
    if(!client) { return PTK_ERR_NULL_PTR; }

    // Create request buffer
    ptk_buf *request_buf;
    ptk_err err = ptk_buf_alloc(&request_buf, 7 + 5);  // MBAP + FC + address + value
    if(err != PTK_OK) { return err; }

    // Create MBAP header
    modbus_mbap_header_t mbap = {.transaction_id = 0,  // Will be filled by send_request
                                 .protocol_id = 0,
                                 .length = 5,  // Unit ID + FC + address + value
                                 .unit_id = client->unit_id};

    ptk_u16_be coil_value = value ? MODBUS_COIL_ON : MODBUS_COIL_OFF;

    err = modbus_mbap_header_encode(request_buf, &mbap);
    if(err == PTK_OK) {
        err = ptk_buf_encode(request_buf, "ptk_u8 ptk_u16_be ptk_u16_be", MODBUS_FC_WRITE_SINGLE_COIL, address, coil_value);
    }

    if(err != PTK_OK) {
        ptk_buf_free(request_buf);
        return err;
    }

    // Send request and get response
    ptk_buf *response_buf;
    err = modbus_client_send_request(client, request_buf, &response_buf);
    if(err != PTK_OK) { return err; }

    if(!response_buf) { return PTK_ERR_PROTOCOL_ERROR; }

    // Parse response (should echo the request)
    ptk_buf_set_cursor(response_buf, 7);  // Skip MBAP header

    ptk_u8 function_code;
    ptk_u16_be resp_address, resp_value;
    err = ptk_buf_decode(response_buf, "ptk_u8 ptk_u16_be ptk_u16_be", &function_code, &resp_address, &resp_value);

    if(err != PTK_OK) {
        ptk_buf_free(response_buf);
        return PTK_ERR_PARSE_ERROR;
    }

    if(function_code & 0x80) {
        // Exception response
        ptk_buf_free(response_buf);
        return PTK_ERR_DEVICE_FAILURE;
    }

    if(function_code != MODBUS_FC_WRITE_SINGLE_COIL || resp_address != address || resp_value != coil_value) {
        ptk_buf_free(response_buf);
        return PTK_ERR_PROTOCOL_ERROR;
    }

    ptk_buf_free(response_buf);
    return PTK_OK;
}

modbus_err_t modbus_client_write_single_register(modbus_client_t *client, ptk_u16_be address, ptk_u16_be value) {
    if(!client) { return PTK_ERR_NULL_PTR; }

    // Create request buffer
    ptk_buf *request_buf;
    ptk_err err = ptk_buf_alloc(&request_buf, 7 + 5);  // MBAP + FC + address + value
    if(err != PTK_OK) { return err; }

    // Create MBAP header
    modbus_mbap_header_t mbap = {.transaction_id = 0,  // Will be filled by send_request
                                 .protocol_id = 0,
                                 .length = 5,  // Unit ID + FC + address + value
                                 .unit_id = client->unit_id};

    err = modbus_mbap_header_encode(request_buf, &mbap);
    if(err == PTK_OK) {
        err = ptk_buf_encode(request_buf, "ptk_u8 ptk_u16_be ptk_u16_be", MODBUS_FC_WRITE_SINGLE_REGISTER, address, value);
    }

    if(err != PTK_OK) {
        ptk_buf_free(request_buf);
        return err;
    }

    // Send request and get response
    ptk_buf *response_buf;
    err = modbus_client_send_request(client, request_buf, &response_buf);
    if(err != PTK_OK) { return err; }

    if(!response_buf) { return PTK_ERR_PROTOCOL_ERROR; }

    // Parse response (should echo the request)
    ptk_buf_set_cursor(response_buf, 7);  // Skip MBAP header

    ptk_u8 function_code;
    ptk_u16_be resp_address, resp_value;
    err = ptk_buf_decode(response_buf, "ptk_u8 ptk_u16_be ptk_u16_be", &function_code, &resp_address, &resp_value);

    if(err != PTK_OK) {
        ptk_buf_free(response_buf);
        return PTK_ERR_PARSE_ERROR;
    }

    if(function_code & 0x80) {
        // Exception response
        ptk_buf_free(response_buf);
        return PTK_ERR_DEVICE_FAILURE;
    }

    if(function_code != MODBUS_FC_WRITE_SINGLE_REGISTER || resp_address != address || resp_value != value) {
        ptk_buf_free(response_buf);
        return PTK_ERR_PROTOCOL_ERROR;
    }

    ptk_buf_free(response_buf);
    return PTK_OK;
}