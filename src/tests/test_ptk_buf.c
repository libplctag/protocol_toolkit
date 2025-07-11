// #include "ptk_buf.h"
// #include <stdio.h>
// #include <string.h>
// #include <assert.h>

// // Simple POD struct for serialization
// typedef struct {
//     uint16_t a;
//     uint32_t b;
//     float c;
// } test_struct_t;

// static ptk_err test_struct_serialize(ptk_buf *buf, ptk_serializable_t *obj) {
//     test_struct_t *s = (test_struct_t *)obj;
//     return ptk_buf_serialize(buf, PTK_BUF_LITTLE_ENDIAN, s->a, s->b, s->c);
// }

// static ptk_err test_struct_deserialize(ptk_buf *buf, ptk_serializable_t *obj) {
//     test_struct_t *s = (test_struct_t *)obj;
//     return ptk_buf_deserialize(buf, false, PTK_BUF_LITTLE_ENDIAN, &s->a, &s->b, &s->c);
// }

void test_serialization() {
    // test_struct_t s = {0x1234, 0xDEADBEEF, 3.14159f};
    // ptk_serializable_t vtable = {test_struct_serialize, test_struct_deserialize};
    // memcpy(&s, &s, sizeof(s));
    //
    // ptk_buf *buf = ptk_buf_alloc(32);
    // assert(buf);
    //
    // // Serialize
    // ptk_err err = vtable.serialize(buf, &vtable);
    // assert(err == PTK_OK);
    // assert(buf->cursor == sizeof(uint16_t) + sizeof(uint32_t) + sizeof(float));
    //
    // // Reset cursor for deserialization
    // buf->cursor = 0;
    // test_struct_t s2 = {0};
    // ptk_serializable_t vtable2 = {test_struct_serialize, test_struct_deserialize};
    // err = vtable2.deserialize(buf, &vtable2);
    // assert(err == PTK_OK);
    //
    // // Check values
    // assert(s.a == s2.a);
    // assert(s.b == s2.b);
    // assert(memcmp(&s.c, &s2.c, sizeof(float)) == 0);
    //
    // ptk_free(buf);
    // printf("test_serialization passed\n");
}

int main() {
    test_serialization();
    // printf("All ptk_buf tests passed.\n");
    return 0;
}
