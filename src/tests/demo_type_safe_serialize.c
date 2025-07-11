#include "ptk_alloc.h"
#include "ptk_buf.h"
#include "ptk_log.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

// EtherNet/IP header structure from the specification
typedef struct {
    uint16_t command;         // EIP command type
    uint16_t length;          // Length of data following header
    uint32_t session_handle;  // Session identifier
    uint32_t status;          // Status/error code
    uint64_t sender_context;  // Client context data (8 bytes)
    uint32_t options;         // Command options
} eip_header_t;

void demonstrate_basic_usage() {
    printf("\n=== Basic Usage Example ===\n");

    ptk_allocator_t *alloc = allocator_arena_create(1024, 8);
    ptk_buf *buf = ptk_buf_create(alloc, 256);

    // Create a Register Session request
    eip_header_t header = {
        .command = 0x0065,                        // Register Session
        .length = 4,                              // 4 bytes of data following
        .session_handle = 0,                      // 0 for register session request
        .status = 0,                              // Success
        .sender_context = 0x123456789ABCDEF0ULL,  // Client context
        .options = 0                              // No options
    };

    printf("Serializing EtherNet/IP Register Session request:\n");
    printf("  Command: 0x%04x (Register Session)\n", header.command);
    printf("  Length: %u bytes\n", header.length);
    printf("  Session Handle: 0x%08x\n", header.session_handle);
    printf("  Status: 0x%08x\n", header.status);
    printf("  Sender Context: 0x%016llx\n", (unsigned long long)header.sender_context);
    printf("  Options: 0x%08x\n", header.options);

    // METHOD 1: Individual field serialization with automatic type detection
    ptk_err err = ptk_buf_serialize(buf, PTK_BUF_LITTLE_ENDIAN, header.command, header.length, header.session_handle,
                                    header.status, header.sender_context, header.options);

    assert(err == PTK_OK);
    printf("\n✓ Serialized using individual fields: %zu bytes\n", ptk_buf_len(buf));

    // Print hex dump
    printf("Hex dump: ");
    uint8_t *data = ptk_buf_get_start_ptr(buf);
    for(size_t i = 0; i < ptk_buf_len(buf); i++) {
        printf("%02x ", data[i]);
        if((i + 1) % 8 == 0) { printf(" "); }
    }
    printf("\n");

    // Deserialize and verify
    eip_header_t received = {0};
    err = ptk_buf_deserialize(buf, false, PTK_BUF_LITTLE_ENDIAN, &received.command, &received.length, &received.session_handle,
                              &received.status, &received.sender_context, &received.options);

    assert(err == PTK_OK);
    assert(received.command == header.command);
    assert(received.length == header.length);
    assert(received.session_handle == header.session_handle);
    assert(received.status == header.status);
    assert(received.sender_context == header.sender_context);
    assert(received.options == header.options);

    printf("✓ Deserialized and verified all fields match\n");

    ptk_buf_dispose(buf);
    ptk_allocator_destroy(alloc);
}

void demonstrate_endian_conversion() {
    printf("\n=== Endianness Conversion Example ===\n");

    ptk_allocator_t *alloc = allocator_arena_create(1024, 8);
    ptk_buf *buf_le = ptk_buf_create(alloc, 256);
    ptk_buf *buf_be = ptk_buf_create(alloc, 256);

    uint32_t test_value = 0x12345678;
    printf("Original value: 0x%08x\n", test_value);

    // Serialize as little-endian (network protocols like EtherNet/IP use little-endian)
    ptk_err err = ptk_buf_serialize(buf_le, PTK_BUF_LITTLE_ENDIAN, test_value);
    assert(err == PTK_OK);

    // Serialize as big-endian (for comparison)
    err = ptk_buf_serialize(buf_be, PTK_BUF_BIG_ENDIAN, test_value);
    assert(err == PTK_OK);

    printf("Little-endian bytes: ");
    uint8_t *le_data = ptk_buf_get_start_ptr(buf_le);
    for(size_t i = 0; i < 4; i++) { printf("%02x ", le_data[i]); }
    printf("(EtherNet/IP format)\n");

    printf("Big-endian bytes:    ");
    uint8_t *be_data = ptk_buf_get_start_ptr(buf_be);
    for(size_t i = 0; i < 4; i++) { printf("%02x ", be_data[i]); }
    printf("(Network byte order)\n");

    // Verify deserialization works correctly
    uint32_t le_result, be_result;
    err = ptk_buf_deserialize(buf_le, false, PTK_BUF_LITTLE_ENDIAN, &le_result);
    assert(err == PTK_OK && le_result == test_value);

    err = ptk_buf_deserialize(buf_be, false, PTK_BUF_BIG_ENDIAN, &be_result);
    assert(err == PTK_OK && be_result == test_value);

    printf("✓ Both endianness formats deserialize correctly to original value\n");

    ptk_buf_dispose(buf_le);
    ptk_buf_dispose(buf_be);
    ptk_allocator_destroy(alloc);
}

void demonstrate_safety_features() {
    printf("\n=== Type Safety and Error Handling ===\n");

    ptk_allocator_t *alloc = allocator_arena_create(1024, 8);
    ptk_buf *buf = ptk_buf_create(alloc, 16);  // Small buffer to trigger overflow

    printf("Testing buffer overflow protection...\n");

    // Try to write more data than buffer can hold
    uint64_t large_val1 = 0x1111111111111111ULL;
    uint64_t large_val2 = 0x2222222222222222ULL;
    uint64_t large_val3 = 0x3333333333333333ULL;  // This will cause overflow

    ptk_err err = ptk_buf_serialize(buf, PTK_BUF_LITTLE_ENDIAN, large_val1, large_val2);
    assert(err == PTK_OK);
    printf("✓ Successfully wrote %zu bytes\n", ptk_buf_len(buf));

    err = ptk_buf_serialize(buf, PTK_BUF_LITTLE_ENDIAN, large_val3);
    assert(err != PTK_OK);
    printf("✓ Buffer overflow correctly detected (error code %d)\n", err);

    // Reset for underflow test
    ptk_buf_set_start(buf, 0);
    ptk_buf_set_end(buf, 0);

    printf("\nTesting buffer underflow protection...\n");

    uint32_t small_val = 0x12345678;
    err = ptk_buf_serialize(buf, PTK_BUF_LITTLE_ENDIAN, small_val);  // Write 4 bytes
    assert(err == PTK_OK);

    // Try to read more than what's available
    uint32_t read_val1;
    uint64_t read_val2;  // This needs 8 bytes but only 4 available

    err = ptk_buf_deserialize(buf, false, PTK_BUF_LITTLE_ENDIAN, &read_val1, &read_val2);
    assert(err != PTK_OK);
    printf("✓ Buffer underflow correctly detected (error code %d)\n", err);

    printf("✓ All safety checks passed - the system prevents common buffer errors\n");

    ptk_buf_dispose(buf);
    ptk_allocator_destroy(alloc);
}

void demonstrate_peek_functionality() {
    printf("\n=== Peek Functionality Example ===\n");

    ptk_allocator_t *alloc = allocator_arena_create(1024, 8);
    ptk_buf *buf = ptk_buf_create(alloc, 256);

    // Simulate receiving an EtherNet/IP header
    eip_header_t incoming = {.command = 0x006F,  // Unconnected Send
                             .length = 16,
                             .session_handle = 0x12345678,
                             .status = 0,
                             .sender_context = 0xFEDCBA9876543210ULL,
                             .options = 0};

    // Serialize the incoming header
    ptk_err err = ptk_buf_serialize(buf, PTK_BUF_LITTLE_ENDIAN, incoming.command, incoming.length, incoming.session_handle,
                                    incoming.status, incoming.sender_context, incoming.options);
    assert(err == PTK_OK);

    printf("Received EtherNet/IP packet (%zu bytes)\n", ptk_buf_len(buf));

    // Peek at the command to determine packet type without consuming data
    uint16_t cmd;
    err = ptk_buf_deserialize(buf, true, PTK_BUF_LITTLE_ENDIAN, &cmd);  // Peek at first field
    assert(err == PTK_OK);

    printf("Peeked at command: 0x%04x ", cmd);
    switch(cmd) {
        case 0x0065: printf("(Register Session)\n"); break;
        case 0x0066: printf("(Unregister Session)\n"); break;
        case 0x006F: printf("(Unconnected Send)\n"); break;
        case 0x0070: printf("(Connected Send)\n"); break;
        default: printf("(Unknown)\n"); break;
    }

    // Buffer position should be unchanged
    assert(ptk_buf_len(buf) == 24);
    printf("✓ Buffer position unchanged after peek (%zu bytes still available)\n", ptk_buf_len(buf));

    // Now actually parse the full header
    eip_header_t parsed = {0};
    err = ptk_buf_deserialize(buf, false, PTK_BUF_LITTLE_ENDIAN, &parsed.command, &parsed.length, &parsed.session_handle,
                              &parsed.status, &parsed.sender_context, &parsed.options);
    assert(err == PTK_OK);

    printf("✓ Parsed full header, buffer now empty (%zu bytes remaining)\n", ptk_buf_len(buf));
    printf("  Parsed command: 0x%04x, length: %u, session: 0x%08x\n", parsed.command, parsed.length, parsed.session_handle);

    ptk_buf_dispose(buf);
    ptk_allocator_destroy(alloc);
}

int main() {
    printf("Protocol Toolkit Type-Safe Buffer Serialization Demo\n");
    printf("===================================================\n");
    printf("Demonstrating the new macro-based serialization system\n");
    printf("designed for EtherNet/IP and other industrial protocols.\n");

    demonstrate_basic_usage();
    demonstrate_endian_conversion();
    demonstrate_safety_features();
    demonstrate_peek_functionality();

    printf("\n🎉 Demo completed successfully!\n");
    printf("\nKey Benefits of the Type-Safe System:\n");
    printf("• Automatic type detection using C11 _Generic\n");
    printf("• Automatic argument counting (no manual count or sentinel values)\n");
    printf("• Compile-time type safety prevents many buffer errors\n");
    printf("• Explicit endianness specification for clarity\n");
    printf("• Peek functionality for protocol parsing\n");
    printf("• Clean, explicit syntax: ptk_buf_serialize(buf, PTK_BUF_LITTLE_ENDIAN, field1, field2, ...)\n");
    printf("• C-idiomatic design with no hidden conveniences\n");

    return 0;
}
