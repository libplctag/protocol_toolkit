// ptk_alloc.c - rewritten for new API (no parent/child, 16-byte alignment, zeroing, destructor)
#include "ptk_alloc.h"
#include "ptk_err.h"
#include "ptk_log.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define PTK_ALLOC_ALIGNMENT 16

typedef struct ptk_alloc_header {
    void (*destructor)(void *ptr);
    size_t size;
    const char *file;
    int line;
} ptk_alloc_header_t;

static size_t ptk_round_up_16(size_t sz) { return (sz + PTK_ALLOC_ALIGNMENT - 1) & ~(PTK_ALLOC_ALIGNMENT - 1); }

static ptk_alloc_header_t *ptk_get_header(void *user_ptr) {
    if(!user_ptr) { return NULL; }
    return (ptk_alloc_header_t *)((uint8_t *)user_ptr - sizeof(ptk_alloc_header_t));
}

void *ptk_alloc_impl(const char *file, int line, size_t size, void (*destructor)(void *ptr)) {
    if(size == 0) {
        warn("ptk_alloc: called with zero size at %s:%d", file, line);
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return NULL;
    }
    size_t user_size = ptk_round_up_16(size);
    size_t total_size = sizeof(ptk_alloc_header_t) + user_size;

    void *raw = NULL;
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    raw = aligned_alloc(PTK_ALLOC_ALIGNMENT, ptk_round_up_16(total_size));
#else
    raw = malloc(ptk_round_up_16(total_size));
#endif
    if(!raw) {
        warn("ptk_alloc: failed to allocate %zu bytes at %s:%d", total_size, file, line);
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return NULL;
    }
    ptk_alloc_header_t *hdr = (ptk_alloc_header_t *)raw;
    hdr->destructor = destructor;
    hdr->size = user_size;
    hdr->file = file;
    hdr->line = line;
    void *user_ptr = (uint8_t *)raw + sizeof(ptk_alloc_header_t);
    memset(user_ptr, 0, user_size);
    info("ptk_alloc: allocated %zu bytes at %p (user: %p) from %s:%d", total_size, raw, user_ptr, file, line);
    ptk_set_err(PTK_OK);
    return user_ptr;
}

void *ptk_realloc_impl(const char *file, int line, void *ptr, size_t new_size) {
    if(!ptr || new_size == 0) {
        warn("ptk_realloc: called with null pointer or zero size at %s:%d", file, line);
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return NULL;
    }
    ptk_alloc_header_t *old_hdr = ptk_get_header(ptr);
    size_t old_size = old_hdr->size;
    size_t new_user_size = ptk_round_up_16(new_size);
    size_t new_total_size = sizeof(ptk_alloc_header_t) + new_user_size;

    void *raw = realloc(old_hdr, ptk_round_up_16(new_total_size));
    if(!raw) {
        warn("ptk_realloc: failed to reallocate to %zu bytes at %s:%d", new_total_size, file, line);
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return NULL;
    }
    ptk_alloc_header_t *new_hdr = (ptk_alloc_header_t *)raw;
    void *new_user_ptr = (uint8_t *)raw + sizeof(ptk_alloc_header_t);
    if(new_user_size > old_size) { memset((uint8_t *)new_user_ptr + old_size, 0, new_user_size - old_size); }
    new_hdr->size = new_user_size;
    new_hdr->file = file;
    new_hdr->line = line;
    info("ptk_realloc: reallocated to %zu bytes at %p (user: %p) from %s:%d", new_total_size, raw, new_user_ptr, file, line);
    ptk_set_err(PTK_OK);
    return new_user_ptr;
}

void ptk_free_impl(const char *file, int line, void *ptr) {
    if(!ptr) {
        warn("ptk_free: called with null pointer at %s:%d", file, line);
        return;
    }
    ptk_alloc_header_t *hdr = ptk_get_header(ptr);
    info("ptk_free: freeing memory at %p (user: %p) allocated from %s:%d", hdr, ptr, hdr ? hdr->file : "?", hdr ? hdr->line : -1);
    if(hdr && hdr->destructor) {
        info("ptk_free: calling destructor for memory at %p (user: %p)", hdr, ptr);
        hdr->destructor(ptr);
    }
    free(hdr);
}
}
}
