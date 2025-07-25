#include "ptk_log.h"

#ifndef NDEBUG
#include "ptk_slice.h"
#include <stdio.h>
#include <stdint.h>
void ptk_log_slice_impl(const char* file, const char* func, int line, const ptk_slice_t* slice) {
    fprintf(stderr, "[%s:%s:%d] slice (len=%zu):\n", file, func, line, (slice)->len);
    const uint8_t* data = (const uint8_t*)(slice)->data;
    size_t len = (slice)->len;
    for (size_t i = 0; i < len; i += 16) {
        fprintf(stderr, "%08zx: ", i);
        size_t row_end = (i + 16 < len) ? (i + 16) : len;
        for (size_t j = i; j < row_end; ++j) {
            fprintf(stderr, "%02X ", data[j]);
        }
        fprintf(stderr, "\n");
    }
    fflush(stderr);
}
#endif
