#include "ptk_types.h"
#include <time.h>
#include <stdint.h>

uint64_t ptk_get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}
