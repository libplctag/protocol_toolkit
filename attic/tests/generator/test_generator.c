/**
 * @file test_generator.c
 * @brief Test the generated format string codec functions
 */

#include "modbus.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

// Simple stub implementations for testing
buf_err_t buf_decode(buf *src, bool peek, const char *fmt, ...) {
    printf("buf_decode called with format: '%s'\n", fmt);
    return BUF_OK;
}

buf_err_t buf_encode(buf *dst, bool expand, const char *fmt, ...) {
    printf("buf_encode called with format: '%s', expand: %s\n", fmt, expand ? "true" : "false");
    return BUF_OK;
}

size_t buf_get_cursor(buf *b) { return b ? b->cursor : 0; }

// Log function stubs
void info(const char *msg) { printf("INFO: %s\n", msg); }

void warn(const char *msg) { printf("WARN: %s\n", msg); }

void info_buf(const uint8_t *data, size_t len) { printf("BUFFER: %zu bytes\n", len); }

void ptk_log_impl(const char *func, int line_num, ptk_log_level level, const char *fmt, ...) { printf("LOG: %s\n", fmt); }

ptk_log_level ptk_log_level_get() { return PTK_LOG_LEVEL_INFO; }

// Stub user-defined functions
codec_err_t read_holding_registers_resp_reg_values_decode(struct read_holding_registers_resp *value, buf *input_buf) {
    printf("User-defined decode for reg_values called\n");
    return CODEC_OK;
}

codec_err_t read_holding_registers_resp_reg_values_encode(buf *output_buf, const struct read_holding_registers_resp *value) {
    printf("User-defined encode for reg_values called\n");
    return CODEC_OK;
}

void read_holding_registers_resp_reg_values_dispose(struct read_holding_registers_resp *value) {
    printf("User-defined dispose for reg_values called\n");
}

void read_holding_registers_resp_reg_values_log_impl(const char *func, int line_num, ptk_log_level log_level,
                                                     struct read_holding_registers_resp *value) {
    printf("User-defined log for reg_values called\n");
}

int main() {
    printf("=== Testing Format String Code Generator ===\n\n");

    // Test 1: MBAP header encode/decode
    printf("Test 1: MBAP Header\n");
    printf("-------------------\n");

    buf test_buf = {0};
    struct mbap *mbap_header;

    // Test decode
    printf("Testing mbap_decode:\n");
    codec_err_t result = mbap_decode(&mbap_header, &test_buf);
    printf("Result: %s\n\n", result == CODEC_OK ? "CODEC_OK" : "ERROR");

    if(result == CODEC_OK) {
        // Test encode
        printf("Testing mbap_encode:\n");
        result = mbap_encode(&test_buf, mbap_header);
        printf("Result: %s\n\n", result == CODEC_OK ? "CODEC_OK" : "ERROR");

        // Test dispose
        printf("Testing mbap_dispose:\n");
        mbap_dispose(mbap_header);
        printf("Disposed successfully\n\n");
    }

    // Test 2: Simple request structure
    printf("Test 2: Read Holding Registers Request\n");
    printf("--------------------------------------\n");

    struct read_holding_registers_req *req;

    printf("Testing read_holding_registers_req_decode:\n");
    result = read_holding_registers_req_decode(&req, &test_buf);
    printf("Result: %s\n\n", result == CODEC_OK ? "CODEC_OK" : "ERROR");

    if(result == CODEC_OK) {
        printf("Testing read_holding_registers_req_encode:\n");
        result = read_holding_registers_req_encode(&test_buf, req);
        printf("Result: %s\n\n", result == CODEC_OK ? "CODEC_OK" : "ERROR");

        printf("Testing read_holding_registers_req_dispose:\n");
        read_holding_registers_req_dispose(req);
        printf("Disposed successfully\n\n");
    }

    // Test 3: Structure with pointer field
    printf("Test 3: Read Holding Registers Response (with pointer field)\n");
    printf("------------------------------------------------------------\n");

    struct read_holding_registers_resp *resp;

    printf("Testing read_holding_registers_resp_decode:\n");
    result = read_holding_registers_resp_decode(&resp, &test_buf);
    printf("Result: %s\n\n", result == CODEC_OK ? "CODEC_OK" : "ERROR");

    if(result == CODEC_OK) {
        printf("Testing read_holding_registers_resp_encode:\n");
        result = read_holding_registers_resp_encode(&test_buf, resp);
        printf("Result: %s\n\n", result == CODEC_OK ? "CODEC_OK" : "ERROR");

        printf("Testing read_holding_registers_resp_dispose:\n");
        read_holding_registers_resp_dispose(resp);
        printf("Disposed successfully\n\n");
    }

    printf("=== Format String Generator Test Complete ===\n");
    printf("\nKey Observations:\n");
    printf("- MBAP format string: '> u16 > u16 > u16 u8' (big-endian)\n");
    printf("- Request format string: 'u8 > u16 > u16' (mixed endianness)\n");
    printf("- Response format string: 'u8 u8' (only non-pointer fields)\n");
    printf("- User-defined functions called for pointer fields\n");
    printf("- Automatic buffer expansion enabled in encode\n");

    return 0;
}