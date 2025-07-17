#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Calculate 16-bit CRC for client-to-server packets
 * 
 * Simple CRC16 implementation for packet integrity checking.
 * 
 * @param data Pointer to data bytes
 * @param length Number of bytes to process
 * @return 16-bit CRC value
 */
uint16_t crc16_calculate(const uint8_t *data, size_t length);

/**
 * @brief Calculate 8-bit CRC for server-to-client packets
 * 
 * Simple CRC8 implementation for packet integrity checking.
 * 
 * @param data Pointer to data bytes  
 * @param length Number of bytes to process
 * @return 8-bit CRC value
 */
uint8_t crc8_calculate(const uint8_t *data, size_t length);