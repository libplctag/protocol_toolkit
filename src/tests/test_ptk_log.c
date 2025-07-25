#include "ptk_log.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "ptk_slice.h"

void test_ptk_log(void) {
#ifndef NDEBUG
    PTK_LOG("Test log: %d", 42);
    uint8_t buf[32];
    for (int i = 0; i < 32; ++i) buf[i] = (uint8_t)i;
    ptk_slice_t s = { .data = buf, .len = sizeof(buf) };
    PTK_LOG_SLICE(s);
#endif
}

int main(void) {
    test_ptk_log();
    return 0;
}
