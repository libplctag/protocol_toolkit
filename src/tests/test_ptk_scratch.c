#include "ptk_scratch.h"
#include <assert.h>
#include <string.h>

void test_ptk_scratch(void) {
    ptk_scratch_t* s = ptk_scratch_create(128);
    assert(s);
    assert(ptk_scratch_capacity(s) == 128);
    assert(ptk_scratch_used(s) == 0);
    ptk_slice_bytes_t a = ptk_scratch_alloc(s, 32);
    assert(a.data && a.len == 32);
    assert(ptk_scratch_used(s) >= 32);
    ptk_slice_bytes_t b = ptk_scratch_alloc_aligned(s, 16, 8);
    assert(b.data && b.len == 16);
    size_t before = ptk_scratch_used(s);
    ptk_scratch_mark_t mark = ptk_scratch_mark(s);
    ptk_slice_bytes_t c = ptk_scratch_alloc(s, 16);
    assert(c.data && c.len == 16);
    ptk_scratch_restore(s, mark);
    assert(ptk_scratch_used(s) == before);
    ptk_scratch_reset(s);
    assert(ptk_scratch_used(s) == 0);
    ptk_scratch_destroy(s);
}

int main(void) {
    test_ptk_scratch();
    return 0;
}
