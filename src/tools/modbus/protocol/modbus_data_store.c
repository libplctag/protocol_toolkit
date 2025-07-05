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
#include <stdint.h>

#include "modbus_defs.h"
#include "ptk_thread.h"
#include "ptk_log.h"

struct modbus_data_store {
    ptk_mutex *mutex;
    
    // Data arrays
    ptk_u8 *coils;
    ptk_u8 *discrete_inputs;
    ptk_u16_be *holding_registers;
    ptk_u16_be *input_registers;
    
    // Configuration
    ptk_u16_be coil_count;
    ptk_u16_be discrete_input_count;
    ptk_u16_be holding_register_count;
    ptk_u16_be input_register_count;
    
    bool read_only_coils;
    bool read_only_holding_registers;
};

modbus_err_t modbus_data_store_create(modbus_data_store_t **store,
                                     const modbus_data_store_config_t *config) {
    if (!store) {
        return PTK_ERR_NULL_PTR;
    }
    
    modbus_data_store_t *new_store = calloc(1, sizeof(modbus_data_store_t));
    if (!new_store) {
        return PTK_ERR_NO_RESOURCES;
    }
    
    // Set configuration or defaults
    if (config) {
        new_store->coil_count = config->coil_count ? config->coil_count : MODBUS_DEFAULT_COIL_COUNT;
        new_store->discrete_input_count = config->discrete_input_count ? config->discrete_input_count : MODBUS_DEFAULT_DISCRETE_INPUT_COUNT;
        new_store->holding_register_count = config->holding_register_count ? config->holding_register_count : MODBUS_DEFAULT_HOLDING_REG_COUNT;
        new_store->input_register_count = config->input_register_count ? config->input_register_count : MODBUS_DEFAULT_INPUT_REG_COUNT;
        new_store->read_only_coils = config->read_only_coils;
        new_store->read_only_holding_registers = config->read_only_holding_registers;
    } else {
        new_store->coil_count = MODBUS_DEFAULT_COIL_COUNT;
        new_store->discrete_input_count = MODBUS_DEFAULT_DISCRETE_INPUT_COUNT;
        new_store->holding_register_count = MODBUS_DEFAULT_HOLDING_REG_COUNT;
        new_store->input_register_count = MODBUS_DEFAULT_INPUT_REG_COUNT;
        new_store->read_only_coils = false;
        new_store->read_only_holding_registers = false;
    }
    
    ptk_err err = ptk_mutex_create(&new_store->mutex, false);
    if (err != PTK_OK) {
        free(new_store);
        return err;
    }
    
    // Allocate data arrays
    // Coils are bit-packed, so we need ceiling division by 8
    size_t coil_bytes = (new_store->coil_count + 7) / 8;
    new_store->coils = calloc(coil_bytes, sizeof(ptk_u8));
    if (!new_store->coils) {
        ptk_mutex_destroy(new_store->mutex);
        free(new_store);
        return PTK_ERR_NO_RESOURCES;
    }
    
    // Discrete inputs are also bit-packed
    size_t discrete_input_bytes = (new_store->discrete_input_count + 7) / 8;
    new_store->discrete_inputs = calloc(discrete_input_bytes, sizeof(ptk_u8));
    if (!new_store->discrete_inputs) {
        free(new_store->coils);
        ptk_mutex_destroy(new_store->mutex);
        free(new_store);
        return PTK_ERR_NO_RESOURCES;
    }
    
    // Holding registers
    new_store->holding_registers = calloc(new_store->holding_register_count, sizeof(ptk_u16_be));
    if (!new_store->holding_registers) {
        free(new_store->discrete_inputs);
        free(new_store->coils);
        ptk_mutex_destroy(new_store->mutex);
        free(new_store);
        return PTK_ERR_NO_RESOURCES;
    }
    
    // Input registers
    new_store->input_registers = calloc(new_store->input_register_count, sizeof(ptk_u16_be));
    if (!new_store->input_registers) {
        free(new_store->holding_registers);
        free(new_store->discrete_inputs);
        free(new_store->coils);
        ptk_mutex_destroy(new_store->mutex);
        free(new_store);
        return PTK_ERR_NO_RESOURCES;
    }
    
    *store = new_store;
    info("Created Modbus data store with %d coils, %d discrete inputs, %d holding registers, %d input registers",
         new_store->coil_count, new_store->discrete_input_count, 
         new_store->holding_register_count, new_store->input_register_count);
    
    return PTK_OK;
}

void modbus_data_store_destroy(modbus_data_store_t *store) {
    if (!store) {
        return;
    }
    
    if (store->mutex) {
        ptk_mutex_destroy(store->mutex);
    }
    
    free(store->coils);
    free(store->discrete_inputs);
    free(store->holding_registers);
    free(store->input_registers);
    free(store);
}

modbus_err_t modbus_data_store_read_coils(modbus_data_store_t *store,
                                         ptk_u16_be address, ptk_u16_be count,
                                         ptk_u8 *values) {
    if (!store || !values) {
        return PTK_ERR_NULL_PTR;
    }
    
    if (count == 0 || count > MODBUS_MAX_COILS) {
        return PTK_ERR_INVALID_PARAM;
    }
    
    if (address + count > store->coil_count) {
        return PTK_ERR_OUT_OF_BOUNDS;
    }
    
    ptk_err err = ptk_mutex_wait_lock(store->mutex, PTK_TIME_WAIT_FOREVER);
    if (err != PTK_OK) {
        return err;
    }
    
    // Copy coil bits to output buffer
    size_t byte_count = (count + 7) / 8;
    memset(values, 0, byte_count);
    
    for (ptk_u16_be i = 0; i < count; i++) {
        ptk_u16_be bit_addr = address + i;
        ptk_u16_be byte_index = bit_addr / 8;
        ptk_u8 bit_index = bit_addr % 8;
        
        if (store->coils[byte_index] & (1 << bit_index)) {
            ptk_u16_be out_byte = i / 8;
            ptk_u8 out_bit = i % 8;
            values[out_byte] |= (1 << out_bit);
        }
    }
    
    ptk_mutex_unlock(store->mutex);
    return PTK_OK;
}

modbus_err_t modbus_data_store_write_coils(modbus_data_store_t *store,
                                          ptk_u16_be address, ptk_u16_be count,
                                          const ptk_u8 *values) {
    if (!store || !values) {
        return PTK_ERR_NULL_PTR;
    }
    
    if (store->read_only_coils) {
        return PTK_ERR_AUTHORIZATION_FAILED;
    }
    
    if (count == 0 || count > MODBUS_MAX_COILS) {
        return PTK_ERR_INVALID_PARAM;
    }
    
    if (address + count > store->coil_count) {
        return PTK_ERR_OUT_OF_BOUNDS;
    }
    
    ptk_err err = ptk_mutex_wait_lock(store->mutex, PTK_TIME_WAIT_FOREVER);
    if (err != PTK_OK) {
        return err;
    }
    
    // Write coil bits from input buffer
    for (ptk_u16_be i = 0; i < count; i++) {
        ptk_u16_be bit_addr = address + i;
        ptk_u16_be byte_index = bit_addr / 8;
        ptk_u8 bit_index = bit_addr % 8;
        
        ptk_u16_be in_byte = i / 8;
        ptk_u8 in_bit = i % 8;
        
        if (values[in_byte] & (1 << in_bit)) {
            store->coils[byte_index] |= (1 << bit_index);
        } else {
            store->coils[byte_index] &= ~(1 << bit_index);
        }
    }
    
    ptk_mutex_unlock(store->mutex);
    return PTK_OK;
}

modbus_err_t modbus_data_store_read_discrete_inputs(modbus_data_store_t *store,
                                                   ptk_u16_be address, ptk_u16_be count,
                                                   ptk_u8 *values) {
    if (!store || !values) {
        return PTK_ERR_NULL_PTR;
    }
    
    if (count == 0 || count > MODBUS_MAX_COILS) {
        return PTK_ERR_INVALID_PARAM;
    }
    
    if (address + count > store->discrete_input_count) {
        return PTK_ERR_OUT_OF_BOUNDS;
    }
    
    ptk_err err = ptk_mutex_wait_lock(store->mutex, PTK_TIME_WAIT_FOREVER);
    if (err != PTK_OK) {
        return err;
    }
    
    // Copy discrete input bits to output buffer
    size_t byte_count = (count + 7) / 8;
    memset(values, 0, byte_count);
    
    for (ptk_u16_be i = 0; i < count; i++) {
        ptk_u16_be bit_addr = address + i;
        ptk_u16_be byte_index = bit_addr / 8;
        ptk_u8 bit_index = bit_addr % 8;
        
        if (store->discrete_inputs[byte_index] & (1 << bit_index)) {
            ptk_u16_be out_byte = i / 8;
            ptk_u8 out_bit = i % 8;
            values[out_byte] |= (1 << out_bit);
        }
    }
    
    ptk_mutex_unlock(store->mutex);
    return PTK_OK;
}

modbus_err_t modbus_data_store_read_holding_registers(modbus_data_store_t *store,
                                                     ptk_u16_be address, ptk_u16_be count,
                                                     ptk_u16_be *values) {
    if (!store || !values) {
        return PTK_ERR_NULL_PTR;
    }
    
    if (count == 0 || count > MODBUS_MAX_REGISTERS) {
        return PTK_ERR_INVALID_PARAM;
    }
    
    if (address + count > store->holding_register_count) {
        return PTK_ERR_OUT_OF_BOUNDS;
    }
    
    ptk_err err = ptk_mutex_wait_lock(store->mutex, PTK_TIME_WAIT_FOREVER);
    if (err != PTK_OK) {
        return err;
    }
    
    // Copy holding registers
    memcpy(values, &store->holding_registers[address], count * sizeof(ptk_u16_be));
    
    ptk_mutex_unlock(store->mutex);
    return PTK_OK;
}

modbus_err_t modbus_data_store_write_holding_registers(modbus_data_store_t *store,
                                                      ptk_u16_be address, ptk_u16_be count,
                                                      const ptk_u16_be *values) {
    if (!store || !values) {
        return PTK_ERR_NULL_PTR;
    }
    
    if (store->read_only_holding_registers) {
        return PTK_ERR_AUTHORIZATION_FAILED;
    }
    
    if (count == 0 || count > MODBUS_MAX_REGISTERS) {
        return PTK_ERR_INVALID_PARAM;
    }
    
    if (address + count > store->holding_register_count) {
        return PTK_ERR_OUT_OF_BOUNDS;
    }
    
    ptk_err err = ptk_mutex_wait_lock(store->mutex, PTK_TIME_WAIT_FOREVER);
    if (err != PTK_OK) {
        return err;
    }
    
    // Write holding registers
    memcpy(&store->holding_registers[address], values, count * sizeof(ptk_u16_be));
    
    ptk_mutex_unlock(store->mutex);
    return PTK_OK;
}

modbus_err_t modbus_data_store_read_input_registers(modbus_data_store_t *store,
                                                   ptk_u16_be address, ptk_u16_be count,
                                                   ptk_u16_be *values) {
    if (!store || !values) {
        return PTK_ERR_NULL_PTR;
    }
    
    if (count == 0 || count > MODBUS_MAX_REGISTERS) {
        return PTK_ERR_INVALID_PARAM;
    }
    
    if (address + count > store->input_register_count) {
        return PTK_ERR_OUT_OF_BOUNDS;
    }
    
    ptk_err err = ptk_mutex_wait_lock(store->mutex, PTK_TIME_WAIT_FOREVER);
    if (err != PTK_OK) {
        return err;
    }
    
    // Copy input registers
    memcpy(values, &store->input_registers[address], count * sizeof(ptk_u16_be));
    
    ptk_mutex_unlock(store->mutex);
    return PTK_OK;
}

const char* modbus_err_string(modbus_err_t err) {
    return ptk_err_to_string(err);
}

void modbus_pack_bits(const ptk_u8 *bits, size_t bit_count, ptk_u8 *bytes) {
    if (!bits || !bytes) {
        return;
    }
    
    size_t byte_count = (bit_count + 7) / 8;
    memset(bytes, 0, byte_count);
    
    for (size_t i = 0; i < bit_count; i++) {
        if (bits[i]) {
            size_t byte_idx = i / 8;
            ptk_u8 bit_idx = i % 8;
            bytes[byte_idx] |= (1 << bit_idx);
        }
    }
}

void modbus_unpack_bits(const ptk_u8 *bytes, size_t bit_count, ptk_u8 *bits) {
    if (!bytes || !bits) {
        return;
    }
    
    for (size_t i = 0; i < bit_count; i++) {
        size_t byte_idx = i / 8;
        ptk_u8 bit_idx = i % 8;
        bits[i] = (bytes[byte_idx] & (1 << bit_idx)) ? 1 : 0;
    }
}