#ifndef PTK_LOG_H
#define PTK_LOG_H

#include <stdio.h>

#ifdef NDEBUG
#define PTK_LOG(fmt, ...) ((void)0)
#else
#define PTK_LOG(fmt, ...) \
    do {fprintf(stderr, "[%s:%s:%d] " fmt "\n", __FILE__, __func__, __LINE__, ##__VA_ARGS__); fflush(stderr);} while(0)
#endif

// Print a byte slice as hex
#ifdef NDEBUG
#define PTK_LOG_SLICE(slice) ((void)0)
#else
#include "ptk_slice.h"
static inline void ptk_log_slice_impl(const char* file, const char* func, int line, const ptk_slice_t* slice) {
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
#define PTK_LOG_SLICE(slice) ptk_log_slice_impl(__FILE__, __func__, __LINE__, &(slice))
#endif

#endif // PTK_LOG_H
