/**
 * PTK - Arena/Scratch Allocator Implementation
 * 
 * Provides fast, predictable memory allocation with no hidden malloc calls.
 * Memory is allocated linearly and can be reset in bulk for reuse.
 */

#include "ptk_scratch.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

extern ptk_status_t ptk_get_last_error(void);
extern void ptk_set_error_internal_internal(ptk_status_t error);

/**
 * Internal scratch allocator structure
 */
struct ptk_scratch {
    uint8_t* memory;        /* Base memory pointer */
    size_t capacity;        /* Total capacity in bytes */
    size_t used;           /* Current used bytes */
};

/**
 * Mark structure for save/restore operations
 */
struct ptk_scratch_mark {
    size_t position;       /* Saved position in arena */
};

/**
 * Set thread-local error (forward declaration from ptk.c)
 */
extern void ptk_clear_error(void);

/**
 * Round up to next multiple of alignment
 */
static size_t align_up(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}


/**
 * Create arena with initial capacity
 * This is the only function that performs actual memory allocation
 */
extern ptk_scratch_t* ptk_scratch_create(size_t initial_capacity) {
    if (initial_capacity == 0) {
        ptk_set_error_internal_internal(PTK_ERROR_INVALID_PARAM);
        return NULL;
    }
    
    /* Allocate the scratch structure */
    ptk_scratch_t* scratch = malloc(sizeof(ptk_scratch_t));
    if (!scratch) {
        ptk_set_error_internal(PTK_ERROR_OUT_OF_MEMORY);
        return NULL;
    }
    
    /* Allocate the memory arena */
    scratch->memory = malloc(initial_capacity);
    if (!scratch->memory) {
        free(scratch);
        ptk_set_error_internal(PTK_ERROR_OUT_OF_MEMORY);
        return NULL;
    }
    
    scratch->capacity = initial_capacity;
    scratch->used = 0;
    
    ptk_clear_error();
    return scratch;
}

/**
 * Reset arena to beginning (fast, no deallocation)
 * All previous allocations become invalid
 */
extern void ptk_scratch_reset(ptk_scratch_t* scratch) {
    if (!scratch) {
        ptk_set_error_internal(PTK_ERROR_INVALID_PARAM);
        return;
    }
    
    scratch->used = 0;
    ptk_clear_error();
}

/**
 * Low-level allocation - returns raw byte slice
 * Returns empty slice on failure
 */
extern ptk_slice_bytes_t ptk_scratch_alloc(ptk_scratch_t* scratch, size_t size) {
    return ptk_scratch_alloc_aligned(scratch, size, 1);
}

/**
 * Aligned allocation - returns raw byte slice
 * Returns empty slice on failure
 */
extern ptk_slice_bytes_t ptk_scratch_alloc_aligned(ptk_scratch_t* scratch, size_t size, size_t alignment) {
    if (!scratch || size == 0) {
        ptk_set_error_internal(PTK_ERROR_INVALID_PARAM);
        return (ptk_slice_bytes_t){.data = NULL, .len = 0};
    }
    
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        /* Alignment must be power of 2 */
        ptk_set_error_internal(PTK_ERROR_INVALID_PARAM);
        return (ptk_slice_bytes_t){.data = NULL, .len = 0};
    }
    
    /* Calculate aligned position */
    size_t aligned_used = align_up(scratch->used, alignment);
    
    /* Check if we have enough space */
    if (aligned_used + size > scratch->capacity) {
        ptk_set_error_internal(PTK_ERROR_OUT_OF_MEMORY);
        return (ptk_slice_bytes_t){.data = NULL, .len = 0};
    }
    
    /* Allocate and update position */
    uint8_t* ptr = scratch->memory + aligned_used;
    scratch->used = aligned_used + size;
    
    ptk_clear_error();
    return (ptk_slice_bytes_t){.data = ptr, .len = size};
}

/**
 * Get current usage stats
 */
extern size_t ptk_scratch_used(ptk_scratch_t* scratch) {
    if (!scratch) {
        ptk_set_error_internal(PTK_ERROR_INVALID_PARAM);
        return 0;
    }
    return scratch->used;
}

extern size_t ptk_scratch_capacity(ptk_scratch_t* scratch) {
    if (!scratch) {
        ptk_set_error_internal(PTK_ERROR_INVALID_PARAM);
        return 0;
    }
    return scratch->capacity;
}

extern size_t ptk_scratch_remaining(ptk_scratch_t* scratch) {
    if (!scratch) {
        ptk_set_error_internal(PTK_ERROR_INVALID_PARAM);
        return 0;
    }
    return scratch->capacity - scratch->used;
}

/**
 * Save/restore position for nested allocations
 * Allows temporary allocations that can be rolled back
 */
extern ptk_scratch_mark_t ptk_scratch_mark(ptk_scratch_t* scratch) {
    ptk_scratch_mark_t mark = {0};
    
    if (!scratch) {
        ptk_set_error_internal(PTK_ERROR_INVALID_PARAM);
        return mark;
    }
    
    mark.position = scratch->used;
    ptk_clear_error();
    return mark;
}

extern void ptk_scratch_restore(ptk_scratch_t* scratch, ptk_scratch_mark_t mark) {
    if (!scratch) {
        ptk_set_error_internal(PTK_ERROR_INVALID_PARAM);
        return;
    }
    
    if (mark.position > scratch->capacity) {
        ptk_set_error_internal(PTK_ERROR_INVALID_PARAM);
        return;
    }
    
    scratch->used = mark.position;
    ptk_clear_error();
}

/**
 * Destroy arena and free all memory
 */
extern void ptk_scratch_destroy(ptk_scratch_t* scratch) {
    if (!scratch) {
        return;  /* Allow NULL pointer, like free() */
    }
    
    if (scratch->memory) {
        free(scratch->memory);
    }
    free(scratch);
}