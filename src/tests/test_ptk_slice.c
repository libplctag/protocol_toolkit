#include "ptk_slice.h"
#include <assert.h>
#include <stdint.h>

void test_ptk_slice(void) {
    uint8_t buf[32];
    for (int i = 0; i < 32; ++i) buf[i] = (uint8_t)i;
    ptk_slice_t s = ptk_slice_make(buf, 32);
    assert(s.data == buf && s.len == 32);
    ptk_slice_t s2 = ptk_slice_advance(s, 8);
    assert(s2.data == buf + 8 && s2.len == 24);
    ptk_slice_t s3 = ptk_slice_truncate(s, 16);
    assert(s3.data == buf && s3.len == 16);
    assert(!ptk_slice_is_empty(s));
    ptk_slice_t s4 = ptk_slice_advance(s, 32);
    assert(s4.len == 0);
}

int main(void) {
    test_ptk_slice();
    return 0;
}
