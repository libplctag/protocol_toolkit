#include "ptk_alloc.h"
#include "ptk_buf.h"
#include "ptk_log.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

// Test structure similar to EtherNet/IP header
typedef struct {
    uint16_t command;         // EIP command type
    uint16_t length;          // Length of data following header
    uint32_t session_handle;  // Session identifier
    uint32_t status;          // Status/error code
    uint64_t sender_context;  // Client context data (8 bytes)
    uint32_t options;         // Command options
} eip_header_t;

// Test PDU structure for serializable interface test
typedef struct {
    ptk_serializable_t base;  // Must be first member
    uint16_t command;
    uint32_t length;
    uint16_t checksum;
} test_pdu_t;

// Forward declarations for test PDU functions
ptk_err test_pdu_serialize(ptk_buf *buf, ptk_serializable_t *obj);
ptk_err test_pdu_deserialize(ptk_buf *buf, ptk_serializable_t *obj);

// Test PDU serialization functions
ptk_err test_pdu_serialize(ptk_buf *buf, ptk_serializable_t *obj) {
    test_pdu_t *pdu = (test_pdu_t *)obj;
    return ptk_buf_serialize(buf, PTK_BUF_LITTLE_ENDIAN, pdu->command, pdu->length, pdu->checksum);
}

ptk_err test_pdu_deserialize(ptk_buf *buf, ptk_serializable_t *obj) {
    test_pdu_t *pdu = (test_pdu_t *)obj;
    return ptk_buf_deserialize(buf, false, PTK_BUF_LITTLE_ENDIAN, &pdu->command, &pdu->length, &pdu->checksum);
}

void test_basic_serialization() {
    printf("\n=== Test Basic Serialization ===\n");

    ptk_buf *buf = ptk_buf_alloc(256);
    assert(buf != NULL);

    // Test data
    uint16_t cmd = 0x0065;
    uint16_t len = 4;
    uint32_t session = 0x12345678;
    uint32_t status = 0;
    uint64_t context = 0x123456789ABCDEF0ULL;
    uint32_t options = 0;

    printf("Original values: cmd=0x%04x, len=%u, session=0x%08x, status=%u, context=0x%016llx, options=%u\n", cmd, len, session,
           status, (unsigned long long)context, options);

    // Test macro-based serialization with little-endian
    ptk_err err = ptk_buf_serialize(buf, PTK_BUF_LITTLE_ENDIAN, cmd, len, session, status, context, options);
    assert(err == PTK_OK);

    printf("Serialized %zu bytes\n", ptk_buf_get_len(buf));

    // Verify buffer contains expected data
    assert(ptk_buf_get_len(buf) == 24);  // 2+2+4+4+8+4 = 24 bytes

    // Print buffer contents in hex
    printf("Buffer contents: ");
    uint8_t *data_ptr = buf->data + buf->start;
    for(size_t i = 0; i < ptk_buf_get_len(buf); i++) { printf("%02x ", data_ptr[i]); }
    printf("\n");

    // Test deserialization
    uint16_t recv_cmd, recv_len;
    uint32_t recv_session, recv_status, recv_options;
    uint64_t recv_context;

    err = ptk_buf_deserialize(buf, false, PTK_BUF_LITTLE_ENDIAN, &recv_cmd, &recv_len, &recv_session, &recv_status, &recv_context,
                              &recv_options);
    assert(err == PTK_OK);

    printf("Deserialized values: cmd=0x%04x, len=%u, session=0x%08x, status=%u, context=0x%016llx, options=%u\n", recv_cmd,
           recv_len, recv_session, recv_status, (unsigned long long)recv_context, recv_options);

    // Verify values match
    assert(recv_cmd == cmd);
    assert(recv_len == len);
    assert(recv_session == session);
    assert(recv_status == status);
    assert(recv_context == context);
    assert(recv_options == options);

    printf("✓ Basic serialization test passed\n");

    ptk_free(&buf);
}

void test_struct_serialization() {
    printf("\n=== Test Struct Serialization ===\n");

    ptk_buf *buf = ptk_buf_alloc(256);
    assert(buf != NULL);

    // Test data
    eip_header_t header = {.command = 0x0065,
                           .length = 4,
                           .session_handle = 0x12345678,
                           .status = 0,
                           .sender_context = 0x123456789ABCDEF0ULL,
                           .options = 0};

    printf("Original struct: cmd=0x%04x, len=%u, session=0x%08x, status=%u, context=0x%016llx, options=%u\n", header.command,
           header.length, header.session_handle, header.status, (unsigned long long)header.sender_context, header.options);

    // Test explicit serialization
    ptk_err err = ptk_buf_serialize(buf, PTK_BUF_LITTLE_ENDIAN, header.command, header.length, header.session_handle,
                                    header.status, header.sender_context, header.options);
    assert(err == PTK_OK);

    printf("Serialized %zu bytes\n", ptk_buf_get_len(buf));
    assert(ptk_buf_get_len(buf) == 24);

    // Test explicit deserialization
    eip_header_t received = {0};
    err = ptk_buf_deserialize(buf, false, PTK_BUF_LITTLE_ENDIAN, &received.command, &received.length, &received.session_handle,
                              &received.status, &received.sender_context, &received.options);
    assert(err == PTK_OK);

    printf("Received struct: cmd=0x%04x, len=%u, session=0x%08x, status=%u, context=0x%016llx, options=%u\n", received.command,
           received.length, received.session_handle, received.status, (unsigned long long)received.sender_context,
           received.options);

    // Verify values match
    assert(received.command == header.command);
    assert(received.length == header.length);
    assert(received.session_handle == header.session_handle);
    assert(received.status == header.status);
    assert(received.sender_context == header.sender_context);
    assert(received.options == header.options);

    printf("✓ Struct serialization test passed\n");

    ptk_free(&buf);
}

void test_endianness() {
    printf("\n=== Test Endianness ===\n");

    ptk_buf *buf1 = ptk_buf_alloc(256);
    ptk_buf *buf2 = ptk_buf_alloc(256);
    assert(buf1 != NULL && buf2 != NULL);

    uint32_t test_value = 0x12345678;

    // Serialize as little-endian
    ptk_err err = ptk_buf_serialize(buf1, PTK_BUF_LITTLE_ENDIAN, test_value);
    assert(err == PTK_OK);

    // Serialize as big-endian
    err = ptk_buf_serialize(buf2, PTK_BUF_BIG_ENDIAN, test_value);
    assert(err == PTK_OK);

    printf("Little-endian bytes: ");
    uint8_t *data1 = buf1->data + buf1->start;
    for(size_t i = 0; i < 4; i++) { printf("%02x ", data1[i]); }
    printf("\n");

    printf("Big-endian bytes: ");
    uint8_t *data2 = buf2->data + buf2->start;
    for(size_t i = 0; i < 4; i++) { printf("%02x ", data2[i]); }
    printf("\n");

// Verify they're different (unless on a big-endian system)
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    assert(memcmp(data1, data2, 4) != 0);
    printf("✓ Endianness conversion verified on little-endian system\n");
#else
    printf("! Running on big-endian system - conversion behavior may differ\n");
#endif

    // Test deserialization with correct endianness
    uint32_t recv_le, recv_be;
    err = ptk_buf_deserialize(buf1, false, PTK_BUF_LITTLE_ENDIAN, &recv_le);
    assert(err == PTK_OK);
    assert(recv_le == test_value);

    err = ptk_buf_deserialize(buf2, false, PTK_BUF_BIG_ENDIAN, &recv_be);
    assert(err == PTK_OK);
    assert(recv_be == test_value);

    printf("✓ Endianness test passed\n");

    ptk_free(&buf1);
    ptk_free(&buf2);
}

void test_peek_functionality() {
    printf("\n=== Test Peek Functionality ===\n");
    fflush(stdout);

    ptk_buf *buf = ptk_buf_alloc(256);
    assert(buf != NULL);

    uint16_t val1 = 0x1234;
    uint32_t val2 = 0x56789ABC;
    printf("DEBUG: About to serialize val1=0x%04x, val2=0x%08x\n", val1, val2);
    fflush(stdout);

    // Serialize data
    ptk_err err = ptk_buf_serialize(buf, PTK_BUF_LITTLE_ENDIAN, val1, val2);
    printf("DEBUG: Serialize returned err=%d\n", err);
    fflush(stdout);
    assert(err == PTK_OK);

    size_t original_size = ptk_buf_get_len(buf);
    printf("Buffer size after serialization: %zu bytes\n", original_size);

    // Peek at the data (should not advance buffer)
    uint16_t peek_val1 = 0xFFFF;
    uint32_t peek_val2 = 0xFFFFFFFF;
    printf("DEBUG: Before peek - peek_val1=0x%04x, peek_val2=0x%08x\n", peek_val1, peek_val2);
    fflush(stdout);
    printf("DEBUG: About to call ptk_buf_deserialize...\n");
    fflush(stdout);
    err = ptk_buf_deserialize(buf, true, PTK_BUF_LITTLE_ENDIAN, &peek_val1, &peek_val2);
    printf("DEBUG: After peek - peek_val1=0x%04x, peek_val2=0x%08x\n", peek_val1, peek_val2);
    printf("DEBUG: Error code from deserialize: %d\n", err);
    fflush(stdout);
    assert(err == PTK_OK);

    printf("Expected: val1=0x%04x, val2=0x%08x\n", val1, val2);
    printf("Peeked:   val1=0x%04x, val2=0x%08x\n", peek_val1, peek_val2);

    printf("DEBUG: About to check val1: expected=0x%04x, actual=0x%04x\n", val1, peek_val1);
    assert(peek_val1 == val1);
    printf("DEBUG: About to check val2: expected=0x%08x, actual=0x%08x\n", val2, peek_val2);
    assert(peek_val2 == val2);

    // Buffer size should be unchanged
    assert(ptk_buf_get_len(buf) == original_size);
    printf("Buffer size after peek: %zu bytes (unchanged)\n", ptk_buf_get_len(buf));

    // Now actually consume the data
    uint16_t real_val1;
    uint32_t real_val2;
    err = ptk_buf_deserialize(buf, false, PTK_BUF_LITTLE_ENDIAN, &real_val1, &real_val2);
    assert(err == PTK_OK);
    assert(real_val1 == val1);
    assert(real_val2 == val2);

    // Buffer should now be empty
    assert(ptk_buf_get_len(buf) == 0);
    printf("Buffer size after consume: %zu bytes (empty)\n", ptk_buf_get_len(buf));

    printf("✓ Peek functionality test passed\n");

    ptk_free(&buf);
}

void test_error_handling() {
    printf("\n=== Test Error Handling ===\n");

    ptk_buf *buf = ptk_buf_alloc(8);  // Small buffer for overflow test
    assert(buf != NULL);

    // Test buffer overflow during serialization
    uint64_t large_val1 = 0x123456789ABCDEF0ULL;
    uint64_t large_val2 = 0xFEDCBA9876543210ULL;

    ptk_err err = ptk_buf_serialize(buf, PTK_BUF_LITTLE_ENDIAN, large_val1);  // Should fit (8 bytes)
    assert(err == PTK_OK);

    err = ptk_buf_serialize(buf, PTK_BUF_LITTLE_ENDIAN, large_val2);  // Should overflow
    assert(err != PTK_OK);
    printf("✓ Buffer overflow correctly detected: error code %d\n", err);

    // Reset buffer for underflow test - manually reset buffer state
    ptk_buf_set_start(buf, 0);
    ptk_buf_set_end(buf, 0);

    // Test buffer underflow during deserialization
    uint32_t small_val = 0x12345678;
    err = ptk_buf_serialize(buf, PTK_BUF_LITTLE_ENDIAN, small_val);  // 4 bytes
    assert(err == PTK_OK);

    uint32_t recv_val;
    uint64_t recv_large;  // Try to read 8 bytes when only 4 available

    err = ptk_buf_deserialize(buf, false, PTK_BUF_LITTLE_ENDIAN, &recv_val, &recv_large);
    assert(err != PTK_OK);
    printf("✓ Buffer underflow correctly detected: error code %d\n", err);

    printf("✓ Error handling test passed\n");

    ptk_free(&buf);
}

void test_serializable_interface() {
    printf("\n=== Test Serializable Interface ===\n");

    ptk_buf *buf = ptk_buf_alloc(256);
    assert(buf != NULL);

    // Initialize test PDU
    test_pdu_t pdu = {.base = {.serialize = test_pdu_serialize, .deserialize = test_pdu_deserialize},
                      .command = 0x1234,
                      .length = 0x56789ABC,
                      .checksum = 0xDEAD};

    printf("Original PDU: cmd=0x%04x, len=0x%08x, checksum=0x%04x\n", pdu.command, pdu.length, pdu.checksum);

    // Test direct PDU serialization
    ptk_err err = ptk_buf_serialize(buf, PTK_BUF_LITTLE_ENDIAN, (ptk_serializable_t *)&pdu);
    assert(err == PTK_OK);

    printf("Serialized PDU: %zu bytes\n", ptk_buf_get_len(buf));
    assert(ptk_buf_get_len(buf) == 8);  // 2+4+2 = 8 bytes

    // Test direct PDU deserialization
    test_pdu_t received_pdu = {.base = {.serialize = test_pdu_serialize, .deserialize = test_pdu_deserialize}};

    err = ptk_buf_deserialize(buf, false, PTK_BUF_LITTLE_ENDIAN, (ptk_serializable_t *)&received_pdu);
    assert(err == PTK_OK);

    printf("Received PDU: cmd=0x%04x, len=0x%08x, checksum=0x%04x\n", received_pdu.command, received_pdu.length,
           received_pdu.checksum);

    // Verify values match
    assert(received_pdu.command == pdu.command);
    assert(received_pdu.length == pdu.length);
    assert(received_pdu.checksum == pdu.checksum);

    // Test mixed serialization (primitives + serializable)
    ptk_buf_set_start(buf, 0);
    ptk_buf_set_end(buf, 0);

    uint8_t preamble = 0xAA;
    uint16_t trailer = 0xBBCC;

    err = ptk_buf_serialize(buf, PTK_BUF_LITTLE_ENDIAN, preamble, (ptk_serializable_t *)&pdu, trailer);
    assert(err == PTK_OK);

    printf("Mixed serialization: %zu bytes\n", ptk_buf_get_len(buf));
    assert(ptk_buf_get_len(buf) == 11);  // 1 + 8 + 2 = 11 bytes

    // Test mixed deserialization
    uint8_t recv_preamble;
    uint16_t recv_trailer;
    test_pdu_t recv_mixed_pdu = {.base = {.serialize = test_pdu_serialize, .deserialize = test_pdu_deserialize}};

    err = ptk_buf_deserialize(buf, false, PTK_BUF_LITTLE_ENDIAN, &recv_preamble, (ptk_serializable_t *)&recv_mixed_pdu,
                              &recv_trailer);
    assert(err == PTK_OK);

    printf("Mixed deserialization: preamble=0x%02x, trailer=0x%04x\n", recv_preamble, recv_trailer);

    assert(recv_preamble == preamble);
    assert(recv_trailer == trailer);
    assert(recv_mixed_pdu.command == pdu.command);
    assert(recv_mixed_pdu.length == pdu.length);
    assert(recv_mixed_pdu.checksum == pdu.checksum);

    printf("✓ Serializable interface test passed\n");

    ptk_free(&buf);
}

int main() {
    // ptk_log_set_level(PTK_LOG_INFO);  // Skip logging for simpler test

    printf("Type-Safe Buffer Serialization Test\n");
    printf("====================================\n");

    test_basic_serialization();
    test_struct_serialization();
    test_serializable_interface();
    test_endianness();
    test_peek_functionality();
    test_error_handling();

    printf("\n🎉 All tests passed! The type-safe serialization system is working correctly.\n");

    return 0;
}
