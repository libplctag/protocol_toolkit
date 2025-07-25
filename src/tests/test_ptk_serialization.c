#include "ptk_serialization.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

void test_ptk_serialization(void) {
    uint8_t buf[32];
    ptk_slice_bytes_t s = { .data = buf, .len = sizeof(buf) };
    s = ptk_serialize_u8(s, 0xAB);
    s = ptk_serialize_i8(s, -42);
    s = ptk_serialize_u16(s, 0x1234, PTK_ENDIAN_BIG);
    s = ptk_serialize_i16(s, -1234, PTK_ENDIAN_LITTLE);
    s = ptk_serialize_u32(s, 0xDEADBEEF, PTK_ENDIAN_BIG);
    s = ptk_serialize_i32(s, -123456, PTK_ENDIAN_LITTLE);
    s = ptk_serialize_u64(s, 0x1122334455667788ULL, PTK_ENDIAN_BIG);
    s = ptk_serialize_i64(s, -9876543210LL, PTK_ENDIAN_LITTLE);
    s = ptk_serialize_f32(s, 3.14f, PTK_ENDIAN_BIG);
    s = ptk_serialize_f64(s, 2.71828, PTK_ENDIAN_LITTLE);
    // Now test deserialization
    ptk_slice_bytes_t r = { .data = buf, .len = sizeof(buf) };
    assert(ptk_deserialize_u8(&r, false) == 0xAB);
    assert(ptk_deserialize_i8(&r, false) == -42);
    assert(ptk_deserialize_u16(&r, false, PTK_ENDIAN_BIG) == 0x1234);
    assert(ptk_deserialize_i16(&r, false, PTK_ENDIAN_LITTLE) == -1234);
    assert(ptk_deserialize_u32(&r, false, PTK_ENDIAN_BIG) == 0xDEADBEEF);
    assert(ptk_deserialize_i32(&r, false, PTK_ENDIAN_LITTLE) == -123456);
    assert(ptk_deserialize_u64(&r, false, PTK_ENDIAN_BIG) == 0x1122334455667788ULL);
    assert(ptk_deserialize_i64(&r, false, PTK_ENDIAN_LITTLE) == -9876543210LL);
    assert(ptk_deserialize_f32(&r, false, PTK_ENDIAN_BIG) == 3.14f);
    assert(ptk_deserialize_f64(&r, false, PTK_ENDIAN_LITTLE) == 2.71828);
}

int main(void) {
    test_ptk_serialization();
    return 0;
}
