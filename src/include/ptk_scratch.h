#ifndef PTK_SCRATCH_H
#define PTK_SCRATCH_H

#include "ptk_types.h"
#include "ptk_slice.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Arena/Scratch Allocator
 * 
 * Provides fast, predictable memory allocation with no hidden malloc calls.
 * Memory is allocated linearly and can be reset in bulk for reuse.
 */

typedef struct ptk_scratch ptk_scratch_t;
typedef struct ptk_scratch_mark ptk_scratch_mark_t;

/**
 * Create arena with initial capacity
 * This is the only function that performs actual memory allocation
 */
ptk_scratch_t* ptk_scratch_create(size_t initial_capacity);

/**
 * Reset arena to beginning (fast, no deallocation)
 * All previous allocations become invalid
 */
void ptk_scratch_reset(ptk_scratch_t* scratch);

/**
 * Low-level allocation - returns raw byte slice
 * Returns empty slice on failure
 */
ptk_slice_bytes_t ptk_scratch_alloc(ptk_scratch_t* scratch, size_t size);
ptk_slice_bytes_t ptk_scratch_alloc_aligned(ptk_scratch_t* scratch, size_t size, size_t alignment);

/**
 * Type-safe allocation macro
 * Usage: ptk_scratch_alloc_slice(scratch, slice_name, count)
 * Example: ptk_scratch_alloc_slice(scratch, u16, 100)
 */
#define ptk_scratch_alloc_slice(scratch, slice_name, count) \
    ptk_slice_##slice_name##_make( \
        (void*)ptk_scratch_alloc_aligned(scratch, \
            ptk_type_info_##slice_name.size * (count), \
            ptk_type_info_##slice_name.alignment).data, \
        (count) \
    )

/**
 * Convenience allocation for byte slices
 */
static inline ptk_slice_t ptk_slice_alloc(ptk_scratch_t* scratch, size_t size) {
    return ptk_scratch_alloc(scratch, size);
}

/**
 * Get current usage stats
 */
size_t ptk_scratch_used(ptk_scratch_t* scratch);
size_t ptk_scratch_capacity(ptk_scratch_t* scratch);
size_t ptk_scratch_remaining(ptk_scratch_t* scratch);

/**
 * Save/restore position for nested allocations
 * Allows temporary allocations that can be rolled back
 */
ptk_scratch_mark_t ptk_scratch_mark(ptk_scratch_t* scratch);
void ptk_scratch_restore(ptk_scratch_t* scratch, ptk_scratch_mark_t mark);

/**
 * Destroy arena and free all memory
 */
void ptk_scratch_destroy(ptk_scratch_t* scratch);

#ifdef __cplusplus
}
#endif

#endif /* PTK_SCRATCH_H */