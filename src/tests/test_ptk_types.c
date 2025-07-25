#include "ptk_types.h"
#include <assert.h>
#include <stdint.h>

void test_ptk_types(void) {
    ptk_status_t st = PTK_OK;
    assert(st == 0);
    ptk_endian_t e = PTK_ENDIAN_HOST;
    assert(e == 2);
    ptk_type_info_t info = { .size = 4, .alignment = 4 };
    assert(info.size == 4 && info.alignment == 4);
    ptk_atomic32_t a = 42;
    assert(a == 42);
}

int main(void) {
    test_ptk_types();
    return 0;
}
