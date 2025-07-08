#ifndef MODBUS_TCP_H
#define MODBUS_TCP_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Modbus Function Codes
#define MODBUS_FUNC_READ_COILS            0x01
#define MODBUS_FUNC_READ_REGS             0x03
#define MODBUS_FUNC_WRITE_SINGLE_COIL     0x05
#define MODBUS_FUNC_WRITE_SINGLE_REG      0x06
#define MODBUS_FUNC_WRITE_MULTIPLE_COILS  0x0F
#define MODBUS_FUNC_WRITE_MULTIPLE_REGS   0x10

// MBAP Header
typedef struct {
    uint16_t transaction_id;
    uint16_t protocol_id;
    uint16_t length;
    uint8_t unit_id;
} MbapHeader;

// Request/Response Stubs
typedef struct {
    uint16_t start_address;
    uint16_t quantity;
} ModbusReadRequest;

typedef struct {
    uint8_t byte_count;
    uint8_t data[252];
} ModbusReadResponse;

typedef struct {
    uint16_t address;
    uint16_t value;
} ModbusWriteSingleRequest;

typedef struct {
    uint16_t address;
    uint16_t value;
} ModbusWriteSingleResponse;

typedef struct {
    uint16_t start_address;
    uint16_t quantity;
    uint8_t byte_count;
    uint8_t data[252];
} ModbusWriteMultipleRequest;

typedef struct {
    uint16_t start_address;
    uint16_t quantity;
} ModbusWriteMultipleResponse;

// Dispatcher (optional)
typedef int (*ModbusHandlerFn)(uint8_t function_code,
                               const uint8_t* pdu_data,
                               size_t pdu_len,
                               uint8_t* response_buf,
                               size_t response_buf_len);

// Public API
int modbus_parse_request(const uint8_t* buffer,
                         size_t length,
                         MbapHeader* header,
                         uint8_t* function_code,
                         const uint8_t** pdu,
                         size_t* pdu_len);

int modbus_build_response(uint8_t function_code,
                          const void* response_pdu,
                          uint8_t* buffer,
                          size_t buffer_len,
                          size_t* out_len);

#ifdef __cplusplus
}
#endif

#endif // MODBUS_TCP_H
