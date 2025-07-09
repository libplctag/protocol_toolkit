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

void test_basic_serialization() {
    printf("\n=== Test Basic Serialization ===\n");

    ptk_allocator_t *alloc = allocator_arena_create(1024, 8);
    assert(alloc != NULL);

    ptk_buf *buf = ptk_buf_create(alloc, 256);
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

    printf("Serialized %zu bytes\n", ptk_buf_len(buf));

    // Verify buffer contains expected data
    assert(ptk_buf_len(buf) == 24);  // 2+2+4+4+8+4 = 24 bytes

    // Print buffer contents in hex
    printf("Buffer contents: ");
    uint8_t *data_ptr = ptk_buf_get_start_ptr(buf);
    for(size_t i = 0; i < ptk_buf_len(buf); i++) { printf("%02x ", data_ptr[i]); }
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

    ptk_buf_dispose(buf);
    ptk_allocator_destroy(alloc);
}

void test_struct_serialization() {
    printf("\n=== Test Struct Serialization ===\n");

    ptk_allocator_t *alloc = allocator_arena_create(1024, 8);
    assert(alloc != NULL);

    ptk_buf *buf = ptk_buf_create(alloc, 256);
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

    printf("Serialized %zu bytes\n", ptk_buf_len(buf));
    assert(ptk_buf_len(buf) == 24);

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

    ptk_buf_dispose(buf);
    ptk_allocator_destroy(alloc);
}

void test_endianness() {
    printf("\n=== Test Endianness ===\n");

    ptk_allocator_t *alloc = allocator_arena_create(1024, 8);
    assert(alloc != NULL);

    ptk_buf *buf1 = ptk_buf_create(alloc, 256);
    ptk_buf *buf2 = ptk_buf_create(alloc, 256);
    assert(buf1 != NULL && buf2 != NULL);

    uint32_t test_value = 0x12345678;

    // Serialize as little-endian
    ptk_err err = ptk_buf_serialize(buf1, PTK_BUF_LITTLE_ENDIAN, test_value);
    assert(err == PTK_OK);

    // Serialize as big-endian
    err = ptk_buf_serialize(buf2, PTK_BUF_BIG_ENDIAN, test_value);
    assert(err == PTK_OK);

    printf("Little-endian bytes: ");
    uint8_t *data1 = ptk_buf_get_start_ptr(buf1);
    for(size_t i = 0; i < 4; i++) { printf("%02x ", data1[i]); }
    printf("\n");

    printf("Big-endian bytes: ");
    uint8_t *data2 = ptk_buf_get_start_ptr(buf2);
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

    ptk_buf_dispose(buf1);
    ptk_buf_dispose(buf2);
    ptk_allocator_destroy(alloc);
}

void test_peek_functionality() {
    printf("\n=== Test Peek Functionality ===\n");

    ptk_allocator_t *alloc = allocator_arena_create(1024, 8);
    assert(alloc != NULL);

    ptk_buf *buf = ptk_buf_create(alloc, 256);
    assert(buf != NULL);

    uint16_t val1 = 0x1234;
    uint32_t val2 = 0x56789ABC;

    // Serialize data
    ptk_err err = ptk_buf_serialize(buf, PTK_BUF_LITTLE_ENDIAN, val1, val2);
    assert(err == PTK_OK);

    size_t original_size = ptk_buf_len(buf);
    printf("Buffer size after serialization: %zu bytes\n", original_size);

    // Peek at the data (should not advance buffer)
    uint16_t peek_val1;
    uint32_t peek_val2;
    err = ptk_buf_deserialize(buf, true, PTK_BUF_LITTLE_ENDIAN, &peek_val1, &peek_val2);
    assert(err == PTK_OK);

    printf("Expected: val1=0x%04x, val2=0x%08x\n", val1, val2);
    printf("Peeked:   val1=0x%04x, val2=0x%08x\n", peek_val1, peek_val2);

    assert(peek_val1 == val1);
    assert(peek_val2 == val2);

    // Buffer size should be unchanged
    assert(ptk_buf_len(buf) == original_size);
    printf("Buffer size after peek: %zu bytes (unchanged)\n", ptk_buf_len(buf));

    // Now actually consume the data
    uint16_t real_val1;
    uint32_t real_val2;
    err = ptk_buf_deserialize(buf, false, PTK_BUF_LITTLE_ENDIAN, &real_val1, &real_val2);
    assert(err == PTK_OK);
    assert(real_val1 == val1);
    assert(real_val2 == val2);

    // Buffer should now be empty
    assert(ptk_buf_len(buf) == 0);
    printf("Buffer size after consume: %zu bytes (empty)\n", ptk_buf_len(buf));

    printf("✓ Peek functionality test passed\n");

    ptk_buf_dispose(buf);
    ptk_allocator_destroy(alloc);
}

void test_error_handling() {
    printf("\n=== Test Error Handling ===\n");

    ptk_allocator_t *alloc = allocator_arena_create(1024, 8);
    assert(alloc != NULL);

    ptk_buf *buf = ptk_buf_create(alloc, 8);  // Small buffer for overflow test
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

    ptk_buf_dispose(buf);
    ptk_allocator_destroy(alloc);
}

int main() {
    // ptk_log_set_level(PTK_LOG_INFO);  // Skip logging for simpler test

    printf("Type-Safe Buffer Serialization Test\n");
    printf("====================================\n");

    test_basic_serialization();
    test_struct_serialization();
    test_endianness();
    test_peek_functionality();
    test_error_handling();

    printf("\n🎉 All tests passed! The type-safe serialization system is working correctly.\n");

    return 0;
}
