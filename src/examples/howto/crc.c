#include "crc.h"

uint16_t crc16_calculate(const uint8_t *data, size_t length) {
    uint16_t crc = 0xFFFF;
    
    for (size_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i];
        
        for (int j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc = crc >> 1;
            }
        }
    }
    
    return crc;
}

uint8_t crc8_calculate(const uint8_t *data, size_t length) {
    uint8_t crc = 0xFF;
    
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07;
            } else {
                crc = crc << 1;
            }
        }
    }
    
    return crc;
}