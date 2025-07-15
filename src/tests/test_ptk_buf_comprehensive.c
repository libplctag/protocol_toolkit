/**
 * @file test_ptk_buf_comprehensive.c  
 * @brief Comprehensive tests for ptk_buf.h API
 *
 * Tests all buffer operations and serialization functionality.
 */

#include <ptk_buf.h>
#include <ptk_log.h>
#include <ptk_mem.h>
#include <stdio.h>
#include <string.h>

//=============================================================================
// Basic Buffer Operation Tests
//=============================================================================

int test_buf_basic_operations(void) {
    info("test_buf_basic_operations entry");
    
    // Test ptk_buf_alloc
    ptk_buf *buf = ptk_buf_alloc(1024);
    if (!buf) {
        error("ptk_buf_alloc failed");
        return 1;
    }
    
    // Test capacity and length
    if (ptk_buf_get_capacity(buf) < 1024) {
        error("Buffer capacity incorrect: %zu < 1024", ptk_buf_get_capacity(buf));
        ptk_local_free(&buf);
        return 2;
    }
    
    if (ptk_buf_get_len(buf) != 0) {
        error("Initial buffer length should be 0, got %zu", ptk_buf_get_len(buf));
        ptk_local_free(&buf);
        return 3;
    }
    
    // Test start/end manipulation
    ptk_buf_set_start(buf, 100);
    ptk_buf_set_end(buf, 200);
    
    if (ptk_buf_get_start(buf) != 100) {
        error("Buffer start position incorrect: %zu != 100", ptk_buf_get_start(buf));
        ptk_local_free(&buf);
        return 4;
    }
    
    if (ptk_buf_get_end(buf) != 200) {
        error("Buffer end position incorrect: %zu != 200", ptk_buf_get_end(buf));
        ptk_local_free(&buf);
        return 5;
    }
    
    if (ptk_buf_get_len(buf) != 100) {
        error("Buffer length should be 100, got %zu", ptk_buf_get_len(buf));
        ptk_local_free(&buf);
        return 6;
    }
    
    ptk_local_free(&buf);
    info("test_buf_basic_operations exit");
    return 0;
}

int test_buf_alloc_from_data(void) {
    info("test_buf_alloc_from_data entry");
    
    const char test_data[] = "Hello, Protocol Toolkit!";
    size_t data_len = strlen(test_data);
    
    ptk_buf *buf = ptk_buf_alloc_from_data((void*)test_data, data_len);
    if (!buf) {
        error("ptk_buf_alloc_from_data failed");
        return 1;
    }
    
    if (ptk_buf_get_len(buf) != data_len) {
        error("Buffer length incorrect: %zu != %zu", ptk_buf_get_len(buf), data_len);
        ptk_local_free(&buf);
        return 2;
    }
    
    // Check that data was copied correctly
    const char *buf_data = (const char*)ptk_buf_get_start(buf);
    if (memcmp(buf_data, test_data, data_len) != 0) {
        error("Buffer data doesn't match source data");
        ptk_local_free(&buf);
        return 3;
    }
    
    ptk_local_free(&buf);
    info("test_buf_alloc_from_data exit");
    return 0;
}

int test_buf_realloc(void) {
    info("test_buf_realloc entry");
    
    ptk_buf *buf = ptk_buf_alloc(100);
    if (!buf) {
        error("ptk_buf_alloc failed");
        return 1;
    }
    
    size_t original_capacity = ptk_buf_get_capacity(buf);
    
    // Add some data
    ptk_buf_set_end(buf, 50);
    for (size_t i = 0; i < 50; i++) {
        ((uint8_t*)ptk_buf_get_start(buf))[i] = (uint8_t)(i & 0xFF);
    }
    
    // Expand buffer
    ptk_err result = ptk_buf_realloc(buf, 2000);
    if (result != PTK_OK) {
        error("ptk_buf_realloc expand failed");
        ptk_local_free(&buf);
        return 2;
    }
    
    if (ptk_buf_get_capacity(buf) < 2000) {
        error("Buffer capacity not expanded: %zu < 2000", ptk_buf_get_capacity(buf));
        ptk_local_free(&buf);
        return 3;
    }
    
    // Verify data integrity
    if (ptk_buf_get_len(buf) != 50) {
        error("Buffer length changed after realloc: %zu != 50", ptk_buf_get_len(buf));
        ptk_local_free(&buf);
        return 4;
    }
    
    for (size_t i = 0; i < 50; i++) {
        uint8_t expected = (uint8_t)(i & 0xFF);
        uint8_t actual = ((uint8_t*)ptk_buf_get_start(buf))[i];
        if (actual != expected) {
            error("Data corrupted at index %zu: %u != %u", i, actual, expected);
            ptk_local_free(&buf);
            return 5;
        }
    }
    
    // Shrink buffer
    result = ptk_buf_realloc(buf, 500);
    if (result != PTK_OK) {
        error("ptk_buf_realloc shrink failed");
        ptk_local_free(&buf);
        return 6;
    }
    
    ptk_local_free(&buf);
    info("test_buf_realloc exit");
    return 0;
}

int test_buf_single_byte_access(void) {
    info("test_buf_single_byte_access entry");
    
    ptk_buf *buf = ptk_buf_alloc(100);
    if (!buf) {
        error("ptk_buf_alloc failed");
        return 1;
    }
    
    // Test setting and getting bytes
    for (int i = 0; i < 10; i++) {
        ptk_err result = ptk_buf_set_u8(buf, (uint8_t)(0x10 + i));
        if (result != PTK_OK) {
            error("ptk_buf_set_u8 failed at iteration %d", i);
            ptk_local_free(&buf);
            return 2;
        }
    }
    
    // Reset position to read
    ptk_buf_set_start(buf, 0);
    
    for (int i = 0; i < 10; i++) {
        uint8_t value;
        ptk_err result = ptk_buf_get_u8(buf, &value);
        if (result != PTK_OK) {
            error("ptk_buf_get_u8 failed at iteration %d", i);
            ptk_local_free(&buf);
            return 3;
        }
        
        uint8_t expected = (uint8_t)(0x10 + i);
        if (value != expected) {
            error("Byte value mismatch at %d: %u != %u", i, value, expected);
            ptk_local_free(&buf);
            return 4;
        }
    }
    
    ptk_local_free(&buf);
    info("test_buf_single_byte_access exit");
    return 0;
}

int test_buf_move_block(void) {
    info("test_buf_move_block entry");
    
    ptk_buf *buf = ptk_buf_alloc(1000);
    if (!buf) {
        error("ptk_buf_alloc failed");
        return 1;
    }
    
    // Fill buffer with test pattern
    ptk_buf_set_start(buf, 100);
    ptk_buf_set_end(buf, 200);
    
    uint8_t *data = (uint8_t*)ptk_buf_get_start(buf);
    for (int i = 0; i < 100; i++) {
        data[i] = (uint8_t)(i & 0xFF);
    }
    
    // Move block to new position
    ptk_err result = ptk_buf_move_block(buf, 300);
    if (result != PTK_OK) {
        error("ptk_buf_move_block failed");
        ptk_local_free(&buf);
        return 2;
    }
    
    // Verify new position
    if (ptk_buf_get_start(buf) != 300) {
        error("Block not moved to correct position: %zu != 300", ptk_buf_get_start(buf));
        ptk_local_free(&buf);
        return 3;
    }
    
    if (ptk_buf_get_end(buf) != 400) {
        error("Block end position incorrect: %zu != 400", ptk_buf_get_end(buf));
        ptk_local_free(&buf);
        return 4;
    }
    
    // Verify data integrity
    data = (uint8_t*)ptk_buf_get_start(buf);
    for (int i = 0; i < 100; i++) {
        uint8_t expected = (uint8_t)(i & 0xFF);
        if (data[i] != expected) {
            error("Data corrupted during move at index %d: %u != %u", i, data[i], expected);
            ptk_local_free(&buf);
            return 5;
        }
    }
    
    ptk_local_free(&buf);
    info("test_buf_move_block exit");
    return 0;
}

//=============================================================================
// Serialization Tests
//=============================================================================

int test_buf_serialize_basic(void) {
    info("test_buf_serialize_basic entry");
    
    ptk_buf *buf = ptk_buf_alloc(1000);
    if (!buf) {
        error("ptk_buf_alloc failed");
        return 1;
    }
    
    // Test serializing various data types
    uint8_t u8_val = 0x42;
    uint16_t u16_val = 0x1234;
    uint32_t u32_val = 0x12345678;
    uint64_t u64_val = 0x123456789ABCDEFULL;
    
    // Serialize in little endian
    ptk_err result = ptk_buf_serialize(buf, PTK_LITTLE_ENDIAN, u8_val, u16_val, u32_val, u64_val);
    if (result != PTK_OK) {
        error("ptk_buf_serialize failed");
        ptk_local_free(&buf);
        return 2;
    }
    
    // Expected total size: 1 + 2 + 4 + 8 = 15 bytes
    if (ptk_buf_get_len(buf) != 15) {
        error("Serialized length incorrect: %zu != 15", ptk_buf_get_len(buf));
        ptk_local_free(&buf);
        return 3;
    }
    
    // Reset for reading
    ptk_buf_set_start(buf, 0);
    
    // Deserialize and verify
    uint8_t read_u8;
    uint16_t read_u16;
    uint32_t read_u32;
    uint64_t read_u64;
    
    result = ptk_buf_deserialize(buf, false, PTK_LITTLE_ENDIAN, &read_u8, &read_u16, &read_u32, &read_u64);
    if (result != PTK_OK) {
        error("ptk_buf_deserialize failed");
        ptk_local_free(&buf);
        return 4;
    }
    
    if (read_u8 != u8_val || read_u16 != u16_val || read_u32 != u32_val || read_u64 != u64_val) {
        error("Deserialized values don't match: %u/%u, %u/%u, %u/%u, %lu/%lu",
              read_u8, u8_val, read_u16, u16_val, read_u32, u32_val, read_u64, u64_val);
        ptk_local_free(&buf);
        return 5;
    }
    
    ptk_local_free(&buf);
    info("test_buf_serialize_basic exit");
    return 0;
}

int test_buf_serialize_endianness(void) {
    info("test_buf_serialize_endianness entry");
    
    ptk_buf *buf = ptk_buf_alloc(100);
    if (!buf) {
        error("ptk_buf_alloc failed");
        return 1;
    }
    
    uint32_t test_value = 0x12345678;
    
    // Serialize as little endian
    ptk_err result = ptk_buf_serialize(buf, PTK_LITTLE_ENDIAN, test_value);
    if (result != PTK_OK) {
        error("Little endian serialize failed");
        ptk_local_free(&buf);
        return 2;
    }
    
    // Check byte order (little endian should be 78 56 34 12)
    uint8_t *data = (uint8_t*)ptk_buf_get_start(buf);
    if (data[0] != 0x78 || data[1] != 0x56 || data[2] != 0x34 || data[3] != 0x12) {
        error("Little endian byte order incorrect: %02x %02x %02x %02x", 
              data[0], data[1], data[2], data[3]);
        ptk_local_free(&buf);
        return 3;
    }
    
    // Clear buffer and test big endian
    ptk_buf_set_start(buf, 0);
    ptk_buf_set_end(buf, 0);
    
    result = ptk_buf_serialize(buf, PTK_BIG_ENDIAN, test_value);
    if (result != PTK_OK) {
        error("Big endian serialize failed");
        ptk_local_free(&buf);
        return 4;
    }
    
    // Check byte order (big endian should be 12 34 56 78)
    data = (uint8_t*)ptk_buf_get_start(buf);
    if (data[0] != 0x12 || data[1] != 0x34 || data[2] != 0x56 || data[3] != 0x78) {
        error("Big endian byte order incorrect: %02x %02x %02x %02x", 
              data[0], data[1], data[2], data[3]);
        ptk_local_free(&buf);
        return 5;
    }
    
    ptk_local_free(&buf);
    info("test_buf_serialize_endianness exit");
    return 0;
}

int test_buf_deserialize_peek(void) {
    info("test_buf_deserialize_peek entry");
    
    ptk_buf *buf = ptk_buf_alloc(100);
    if (!buf) {
        error("ptk_buf_alloc failed");
        return 1;
    }
    
    uint16_t values[] = {0x1111, 0x2222, 0x3333};
    
    // Serialize multiple values
    ptk_err result = ptk_buf_serialize(buf, PTK_LITTLE_ENDIAN, values[0], values[1], values[2]);
    if (result != PTK_OK) {
        error("Serialize failed");
        ptk_local_free(&buf);
        return 2;
    }
    
    // Reset for reading
    ptk_buf_set_start(buf, 0);
    size_t original_start = ptk_buf_get_start(buf);
    
    // Peek at first value (should not advance position)
    uint16_t peeked_value;
    result = ptk_buf_deserialize(buf, true, PTK_LITTLE_ENDIAN, &peeked_value);
    if (result != PTK_OK) {
        error("Peek deserialize failed");
        ptk_local_free(&buf);
        return 3;
    }
    
    if (peeked_value != values[0]) {
        error("Peeked value incorrect: %u != %u", peeked_value, values[0]);
        ptk_local_free(&buf);
        return 4;
    }
    
    // Position should not have advanced
    if (ptk_buf_get_start(buf) != original_start) {
        error("Buffer position advanced during peek: %zu != %zu", 
              ptk_buf_get_start(buf), original_start);
        ptk_local_free(&buf);
        return 5;
    }
    
    // Now read normally (should advance position)
    uint16_t read_value;
    result = ptk_buf_deserialize(buf, false, PTK_LITTLE_ENDIAN, &read_value);
    if (result != PTK_OK) {
        error("Normal deserialize failed");
        ptk_local_free(&buf);
        return 6;
    }
    
    if (read_value != values[0]) {
        error("Read value incorrect: %u != %u", read_value, values[0]);
        ptk_local_free(&buf);
        return 7;
    }
    
    // Position should have advanced
    if (ptk_buf_get_start(buf) == original_start) {
        error("Buffer position did not advance during normal read");
        ptk_local_free(&buf);
        return 8;
    }
    
    ptk_local_free(&buf);
    info("test_buf_deserialize_peek exit");
    return 0;
}

int test_buf_byte_swap(void) {
    info("test_buf_byte_swap entry");
    
    // Test 32-bit byte swap
    uint32_t val32 = 0x12345678;
    uint32_t swapped32 = ptk_buf_byte_swap_u32(val32);
    if (swapped32 != 0x78563412) {
        error("32-bit byte swap failed: 0x%08x != 0x78563412", swapped32);
        return 1;
    }
    
    // Test 64-bit byte swap
    uint64_t val64 = 0x123456789ABCDEFULL;
    uint64_t swapped64 = ptk_buf_byte_swap_u64(val64);
    if (swapped64 != 0xEFCDAB8967452301ULL) {
        error("64-bit byte swap failed: 0x%016lx != 0xEFCDAB8967452301", swapped64);
        return 2;
    }
    
    // Test double swap returns original
    if (ptk_buf_byte_swap_u32(swapped32) != val32) {
        error("Double 32-bit swap doesn't return original");
        return 3;
    }
    
    if (ptk_buf_byte_swap_u64(swapped64) != val64) {
        error("Double 64-bit swap doesn't return original");
        return 4;
    }
    
    info("test_buf_byte_swap exit");
    return 0;
}

//=============================================================================
// Main Test Function
//=============================================================================

int test_ptk_buf_main(void) {
    info("=== Starting PTK Buffer Management Tests ===");
    
    int result = 0;
    
    // Basic buffer operations
    if ((result = test_buf_basic_operations()) != 0) {
        error("test_buf_basic_operations failed with code %d", result);
        return result;
    }
    
    if ((result = test_buf_alloc_from_data()) != 0) {
        error("test_buf_alloc_from_data failed with code %d", result);
        return result;
    }
    
    if ((result = test_buf_realloc()) != 0) {
        error("test_buf_realloc failed with code %d", result);
        return result;
    }
    
    if ((result = test_buf_single_byte_access()) != 0) {
        error("test_buf_single_byte_access failed with code %d", result);
        return result;
    }
    
    if ((result = test_buf_move_block()) != 0) {
        error("test_buf_move_block failed with code %d", result);
        return result;
    }
    
    // Serialization tests
    if ((result = test_buf_serialize_basic()) != 0) {
        error("test_buf_serialize_basic failed with code %d", result);
        return result;
    }
    
    if ((result = test_buf_serialize_endianness()) != 0) {
        error("test_buf_serialize_endianness failed with code %d", result);
        return result;
    }
    
    if ((result = test_buf_deserialize_peek()) != 0) {
        error("test_buf_deserialize_peek failed with code %d", result);
        return result;
    }
    
    if ((result = test_buf_byte_swap()) != 0) {
        error("test_buf_byte_swap failed with code %d", result);
        return result;
    }
    
    info("=== All PTK Buffer Management Tests Passed ===");
    return 0;
}