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

#include "modbus_defs.h"
#include "ptk_log.h"

modbus_err_t modbus_mbap_header_encode(ptk_buf *dest, const modbus_mbap_header_t *header) {
    if(!dest || !header) { return PTK_ERR_NULL_PTR; }

    ptk_err err;

    err = ptk_buf_encode(dest, "ptk_u16_be ptk_u16_be ptk_u16_be ptk_u8", header->transaction_id, header->protocol_id,
                         header->length, header->unit_id);

    return err;
}

modbus_err_t modbus_mbap_header_decode(modbus_mbap_header_t *header, ptk_buf *src) {
    if(!header || !src) { return PTK_ERR_NULL_PTR; }

    ptk_err err;

    err = ptk_buf_decode(src, "ptk_u16_be ptk_u16_be ptk_u16_be ptk_u8", &header->transaction_id, &header->protocol_id,
                         &header->length, &header->unit_id);

    return err;
}

modbus_err_t modbus_read_holding_registers_req_encode(ptk_buf *dest, const modbus_read_holding_registers_req_t *req) {
    if(!dest || !req) { return PTK_ERR_NULL_PTR; }

    ptk_err err;

    err = ptk_buf_encode(dest, "ptk_u8 ptk_u16_be ptk_u16_be", req->function_code, req->starting_address,
                         req->quantity_of_registers);

    return err;
}

modbus_err_t modbus_read_holding_registers_req_decode(modbus_read_holding_registers_req_t *req, ptk_buf *src) {
    if(!req || !src) { return PTK_ERR_NULL_PTR; }

    ptk_err err;

    err = ptk_buf_decode(src, "ptk_u8 ptk_u16_be ptk_u16_be", &req->function_code, &req->starting_address,
                         &req->quantity_of_registers);

    return err;
}

modbus_err_t modbus_read_holding_registers_resp_encode(ptk_buf *dest, const modbus_read_holding_registers_resp_t *resp) {
    if(!dest || !resp) { return PTK_ERR_NULL_PTR; }

    ptk_err err;

    // Encode function code and byte count
    err = ptk_buf_encode(dest, "ptk_u8 ptk_u8", resp->function_code, resp->byte_count);

    if(err != PTK_OK) { return err; }

    // Encode register values as array
    for(int i = 0; i < resp->byte_count / 2; i++) {
        err = ptk_buf_encode(dest, "ptk_u16_be", resp->register_values[i]);
        if(err != PTK_OK) { return err; }
    }

    return PTK_OK;
}

static modbus_err_t modbus_create_exception_response(ptk_buf **response_buf, ptk_u8 function_code, ptk_u8 exception_code) {
    if(!response_buf) { return PTK_ERR_NULL_PTR; }

    ptk_err err = ptk_buf_alloc(response_buf, 7 + 2);  // MBAP header + exception response
    if(err != PTK_OK) { return err; }

    // Create MBAP header for exception response
    modbus_mbap_header_t mbap = {
        .transaction_id = 0,  // Will be filled by caller
        .protocol_id = 0,
        .length = 3,  // Unit ID + function code + exception code
        .unit_id = 0  // Will be filled by caller
    };

    err = modbus_mbap_header_encode(*response_buf, &mbap);
    if(err != PTK_OK) {
        ptk_buf_free(*response_buf);
        *response_buf = NULL;
        return err;
    }

    // Encode exception response
    err = ptk_buf_encode(*response_buf, "ptk_u8 ptk_u8", function_code | 0x80, exception_code);

    if(err != PTK_OK) {
        ptk_buf_free(*response_buf);
        *response_buf = NULL;
        return err;
    }

    return PTK_OK;
}

modbus_err_t modbus_process_request(modbus_data_store_t *data_store, ptk_buf *request_buf, ptk_buf **response_buf,
                                    ptk_u8 unit_id) {
    if(!data_store || !request_buf || !response_buf) { return PTK_ERR_NULL_PTR; }

    *response_buf = NULL;

    // Reset buffer cursor to start
    ptk_buf_set_cursor(request_buf, 0);

    // Decode MBAP header
    modbus_mbap_header_t mbap;
    ptk_err err = modbus_mbap_header_decode(&mbap, request_buf);
    if(err != PTK_OK) {
        error("Failed to decode MBAP header: %s", ptk_err_to_string(err));
        return err;
    }

    // Validate protocol ID
    if(mbap.protocol_id != 0) {
        error("Invalid protocol ID: %d", mbap.protocol_id);
        return PTK_ERR_PROTOCOL_ERROR;
    }

    // Check unit ID
    if(mbap.unit_id != unit_id) {
        debug("Unit ID mismatch: expected %d, got %d", unit_id, mbap.unit_id);
        return PTK_ERR_PROTOCOL_ERROR;
    }

    // Read function code
    ptk_u8 function_code;
    err = ptk_buf_decode(request_buf, false, "ptk_u8", &function_code);
    if(err != PTK_OK) { return modbus_create_exception_response(response_buf, function_code, MODBUS_EX_ILLEGAL_FUNCTION); }

    debug("Processing Modbus function code 0x%02X", function_code);

    switch(function_code) {
        case MODBUS_FC_READ_COILS: {
            modbus_read_coils_req_t req;
            req.function_code = function_code;

            err = ptk_buf_decode(request_buf, "ptk_u16_be ptk_u16_be", &req.starting_address, &req.quantity_of_coils);
            if(err != PTK_OK) {
                return modbus_create_exception_response(response_buf, function_code, MODBUS_EX_ILLEGAL_DATA_VALUE);
            }

            if(req.quantity_of_coils == 0 || req.quantity_of_coils > MODBUS_MAX_COILS) {
                return modbus_create_exception_response(response_buf, function_code, MODBUS_EX_ILLEGAL_DATA_VALUE);
            }

            // Calculate response size
            ptk_u8 byte_count = (req.quantity_of_coils + 7) / 8;
            ptk_u8 *coil_data = malloc(byte_count);
            if(!coil_data) {
                return modbus_create_exception_response(response_buf, function_code, MODBUS_EX_SLAVE_DEVICE_FAILURE);
            }

            err = modbus_data_store_read_coils(data_store, req.starting_address, req.quantity_of_coils, coil_data);
            if(err != PTK_OK) {
                free(coil_data);
                ptk_u8 exception_code =
                    (err == PTK_ERR_OUT_OF_BOUNDS) ? MODBUS_EX_ILLEGAL_DATA_ADDRESS : MODBUS_EX_SLAVE_DEVICE_FAILURE;
                return modbus_create_exception_response(response_buf, function_code, exception_code);
            }

            // Create response buffer
            err = ptk_buf_alloc(response_buf, 7 + 2 + byte_count);  // MBAP + FC + byte count + data
            if(err != PTK_OK) {
                free(coil_data);
                return err;
            }

            // Create response MBAP header
            modbus_mbap_header_t resp_mbap = {.transaction_id = mbap.transaction_id,
                                              .protocol_id = 0,
                                              .length = 2 + byte_count,  // Unit ID + FC + byte count + data
                                              .unit_id = unit_id};

            err = modbus_mbap_header_encode(*response_buf, &resp_mbap);
            if(err == PTK_OK) { err = ptk_buf_encode(*response_buf, "ptk_u8 ptk_u8", function_code, byte_count); }

            // Add coil data
            for(int i = 0; i < byte_count && err == PTK_OK; i++) { err = ptk_buf_encode(*response_buf, "ptk_u8", coil_data[i]); }

            free(coil_data);

            if(err != PTK_OK) {
                ptk_buf_free(*response_buf);
                *response_buf = NULL;
                return err;
            }

            break;
        }

        case MODBUS_FC_READ_HOLDING_REGISTERS: {
            modbus_read_holding_registers_req_t req;
            req.function_code = function_code;

            err = modbus_read_holding_registers_req_decode(&req, request_buf);
            if(err != PTK_OK) {
                return modbus_create_exception_response(response_buf, function_code, MODBUS_EX_ILLEGAL_DATA_VALUE);
            }

            if(req.quantity_of_registers == 0 || req.quantity_of_registers > MODBUS_MAX_REGISTERS) {
                return modbus_create_exception_response(response_buf, function_code, MODBUS_EX_ILLEGAL_DATA_VALUE);
            }

            // Allocate memory for register data
            ptk_u16_be *register_data = malloc(req.quantity_of_registers * sizeof(ptk_u16_be));
            if(!register_data) {
                return modbus_create_exception_response(response_buf, function_code, MODBUS_EX_SLAVE_DEVICE_FAILURE);
            }

            err = modbus_data_store_read_holding_registers(data_store, req.starting_address, req.quantity_of_registers,
                                                           register_data);
            if(err != PTK_OK) {
                free(register_data);
                ptk_u8 exception_code =
                    (err == PTK_ERR_OUT_OF_BOUNDS) ? MODBUS_EX_ILLEGAL_DATA_ADDRESS : MODBUS_EX_SLAVE_DEVICE_FAILURE;
                return modbus_create_exception_response(response_buf, function_code, exception_code);
            }

            // Create response
            ptk_u8 byte_count = req.quantity_of_registers * 2;

            err = ptk_buf_alloc(response_buf, 7 + 2 + byte_count);  // MBAP + FC + byte count + data
            if(err != PTK_OK) {
                free(register_data);
                return err;
            }

            // Create response MBAP header
            modbus_mbap_header_t resp_mbap = {.transaction_id = mbap.transaction_id,
                                              .protocol_id = 0,
                                              .length = 2 + byte_count,  // Unit ID + FC + byte count + data
                                              .unit_id = unit_id};

            err = modbus_mbap_header_encode(*response_buf, &resp_mbap);
            if(err == PTK_OK) { err = ptk_buf_encode(*response_buf, "ptk_u8 ptk_u8", function_code, byte_count); }

            // Add register data
            for(int i = 0; i < req.quantity_of_registers && err == PTK_OK; i++) {
                err = ptk_buf_encode(*response_buf, "ptk_u16_be", register_data[i]);
            }

            free(register_data);

            if(err != PTK_OK) {
                ptk_buf_free(*response_buf);
                *response_buf = NULL;
                return err;
            }

            break;
        }

        case MODBUS_FC_WRITE_SINGLE_COIL: {
            modbus_write_single_coil_req_t req;
            req.function_code = function_code;

            err = ptk_buf_decode(request_buf, "ptk_u16_be ptk_u16_be", &req.output_address, &req.output_value);
            if(err != PTK_OK) {
                return modbus_create_exception_response(response_buf, function_code, MODBUS_EX_ILLEGAL_DATA_VALUE);
            }

            // Validate coil value
            if(req.output_value != MODBUS_COIL_ON && req.output_value != MODBUS_COIL_OFF) {
                return modbus_create_exception_response(response_buf, function_code, MODBUS_EX_ILLEGAL_DATA_VALUE);
            }

            // Convert to bit value
            ptk_u8 coil_value = (req.output_value == MODBUS_COIL_ON) ? 1 : 0;

            err = modbus_data_store_write_coils(data_store, req.output_address, 1, &coil_value);
            if(err != PTK_OK) {
                ptk_u8 exception_code;
                switch(err) {
                    case PTK_ERR_OUT_OF_BOUNDS: exception_code = MODBUS_EX_ILLEGAL_DATA_ADDRESS; break;
                    case PTK_ERR_AUTHORIZATION_FAILED: exception_code = MODBUS_EX_ILLEGAL_FUNCTION; break;
                    default: exception_code = MODBUS_EX_SLAVE_DEVICE_FAILURE; break;
                }
                return modbus_create_exception_response(response_buf, function_code, exception_code);
            }

            // Echo the request as response
            err = ptk_buf_alloc(response_buf, 7 + 5);  // MBAP + FC + address + value
            if(err != PTK_OK) { return err; }

            modbus_mbap_header_t resp_mbap = {.transaction_id = mbap.transaction_id,
                                              .protocol_id = 0,
                                              .length = 5,  // Unit ID + FC + address + value
                                              .unit_id = unit_id};

            err = modbus_mbap_header_encode(*response_buf, &resp_mbap);
            if(err == PTK_OK) {
                err = ptk_buf_encode(*response_buf, "ptk_u8 ptk_u16_be ptk_u16_be", function_code, req.output_address,
                                     req.output_value);
            }

            if(err != PTK_OK) {
                ptk_buf_free(*response_buf);
                *response_buf = NULL;
                return err;
            }

            break;
        }

        case MODBUS_FC_WRITE_SINGLE_REGISTER: {
            modbus_write_single_register_req_t req;
            req.function_code = function_code;

            err = ptk_buf_decode(request_buf, "ptk_u16_be ptk_u16_be", &req.register_address, &req.register_value);
            if(err != PTK_OK) {
                return modbus_create_exception_response(response_buf, function_code, MODBUS_EX_ILLEGAL_DATA_VALUE);
            }

            err = modbus_data_store_write_holding_registers(data_store, req.register_address, 1, &req.register_value);
            if(err != PTK_OK) {
                ptk_u8 exception_code;
                switch(err) {
                    case PTK_ERR_OUT_OF_BOUNDS: exception_code = MODBUS_EX_ILLEGAL_DATA_ADDRESS; break;
                    case PTK_ERR_AUTHORIZATION_FAILED: exception_code = MODBUS_EX_ILLEGAL_FUNCTION; break;
                    default: exception_code = MODBUS_EX_SLAVE_DEVICE_FAILURE; break;
                }
                return modbus_create_exception_response(response_buf, function_code, exception_code);
            }

            // Echo the request as response
            err = ptk_buf_alloc(response_buf, 7 + 5);  // MBAP + FC + address + value
            if(err != PTK_OK) { return err; }

            modbus_mbap_header_t resp_mbap = {.transaction_id = mbap.transaction_id,
                                              .protocol_id = 0,
                                              .length = 5,  // Unit ID + FC + address + value
                                              .unit_id = unit_id};

            err = modbus_mbap_header_encode(*response_buf, &resp_mbap);
            if(err == PTK_OK) {
                err = ptk_buf_encode(*response_buf, "ptk_u8 ptk_u16_be ptk_u16_be", function_code, req.register_address,
                                     req.register_value);
            }

            if(err != PTK_OK) {
                ptk_buf_free(*response_buf);
                *response_buf = NULL;
                return err;
            }

            break;
        }

        default:
            warn("Unsupported Modbus function code: 0x%02X", function_code);
            return modbus_create_exception_response(response_buf, function_code, MODBUS_EX_ILLEGAL_FUNCTION);
    }

    return PTK_OK;
}
