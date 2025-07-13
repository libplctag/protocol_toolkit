// ptk_alloc.c - rewritten for new API (no parent/child, 16-byte alignment, zeroing, destructor)
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ptk_alloc.h>
#include <ptk_err.h>
#include <ptk_log.h>

#define PTK_ALLOC_ALIGNMENT 16
#define PTK_ALLOC_HEADER_CANARY 0xDEADBEEFCAFEBABEULL
#define PTK_ALLOC_FOOTER_CANARY 0xFEEDFACEDEADC0DEULL

typedef struct ptk_alloc_header {
    uint64_t header_canary;         // Must be first for detection
    void (*destructor)(void *ptr);
    size_t size;
    const char *file;
    int line;
} ptk_alloc_header_t;

typedef struct ptk_alloc_footer {
    uint64_t footer_canary;
} ptk_alloc_footer_t;

static size_t ptk_round_up_16(size_t sz) { return (sz + PTK_ALLOC_ALIGNMENT - 1) & ~(PTK_ALLOC_ALIGNMENT - 1); }

static ptk_alloc_header_t *ptk_get_header(void *user_ptr) {
    if(!user_ptr) { return NULL; }
    return (ptk_alloc_header_t *)((uint8_t *)user_ptr - sizeof(ptk_alloc_header_t));
}

static ptk_alloc_footer_t *ptk_get_footer(ptk_alloc_header_t *header) {
    if(!header) { return NULL; }
    uint8_t *user_ptr = (uint8_t *)header + sizeof(ptk_alloc_header_t);
    return (ptk_alloc_footer_t *)(user_ptr + header->size);
}

static bool ptk_validate_canaries(void *user_ptr) {
    if(!user_ptr) {
        ptk_set_err(PTK_ERR_NULL_PTR);
        return false;
    }
    
    ptk_alloc_header_t *header = ptk_get_header(user_ptr);
    
    // Check if this looks like it could be a valid PTK allocation
    // First check: is the header canary correct?
    if(header->header_canary != PTK_ALLOC_HEADER_CANARY) {
        error("Invalid header canary detected at %p - expected 0x%llx, got 0x%llx", 
              user_ptr, (unsigned long long)PTK_ALLOC_HEADER_CANARY, 
              (unsigned long long)header->header_canary);
        error("This pointer was likely allocated with malloc() instead of ptk_alloc()");
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return false;
    }
    
    // Check footer canary
    ptk_alloc_footer_t *footer = ptk_get_footer(header);
    if(footer->footer_canary != PTK_ALLOC_FOOTER_CANARY) {
        error("Invalid footer canary detected at %p - expected 0x%llx, got 0x%llx",
              user_ptr, (unsigned long long)PTK_ALLOC_FOOTER_CANARY,
              (unsigned long long)footer->footer_canary);
        error("Memory corruption detected or invalid pointer from %s:%d", 
              header->file, header->line);
        ptk_set_err(PTK_ERR_VALIDATION);
        return false;
    }
    
    return true;
}

void *ptk_alloc_impl(const char *file, int line, size_t size, void (*destructor)(void *ptr)) {
    if(size == 0) {
        warn("ptk_alloc: called with zero size at %s:%d", file, line);
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return NULL;
    }
    size_t user_size = ptk_round_up_16(size);
    size_t total_size = sizeof(ptk_alloc_header_t) + user_size + sizeof(ptk_alloc_footer_t);

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
    
    // Initialize header with canary
    ptk_alloc_header_t *hdr = (ptk_alloc_header_t *)raw;
    hdr->header_canary = PTK_ALLOC_HEADER_CANARY;
    hdr->destructor = destructor;
    hdr->size = user_size;
    hdr->file = file;
    hdr->line = line;
    
    // Initialize user data area
    void *user_ptr = (uint8_t *)raw + sizeof(ptk_alloc_header_t);
    memset(user_ptr, 0, user_size);
    
    // Initialize footer with canary
    ptk_alloc_footer_t *footer = ptk_get_footer(hdr);
    footer->footer_canary = PTK_ALLOC_FOOTER_CANARY;
    
    debug("ptk_alloc: allocated %zu bytes at %p (user: %p) from %s:%d", total_size, raw, user_ptr, file, line);
    ptk_set_err(PTK_OK);
    return user_ptr;
}

void *ptk_realloc_impl(const char *file, int line, void *ptr, size_t new_size) {
    if(!ptr || new_size == 0) {
        warn("ptk_realloc: called with null pointer or zero size at %s:%d", file, line);
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return NULL;
    }
    
    // Validate canaries before proceeding
    if(!ptk_validate_canaries(ptr)) {
        error("ptk_realloc: canary validation failed for pointer %p at %s:%d", ptr, file, line);
        return NULL;
    }
    
    ptk_alloc_header_t *old_hdr = ptk_get_header(ptr);
    size_t old_size = old_hdr->size;
    size_t new_user_size = ptk_round_up_16(new_size);
    size_t new_total_size = sizeof(ptk_alloc_header_t) + new_user_size + sizeof(ptk_alloc_footer_t);

    void *raw = realloc(old_hdr, ptk_round_up_16(new_total_size));
    if(!raw) {
        warn("ptk_realloc: failed to reallocate to %zu bytes at %s:%d", new_total_size, file, line);
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return NULL;
    }
    
    ptk_alloc_header_t *new_hdr = (ptk_alloc_header_t *)raw;
    void *new_user_ptr = (uint8_t *)raw + sizeof(ptk_alloc_header_t);
    
    // Zero new memory if size increased
    if(new_user_size > old_size) { 
        memset((uint8_t *)new_user_ptr + old_size, 0, new_user_size - old_size); 
    }
    
    // Update header (canary should still be intact)
    new_hdr->size = new_user_size;
    new_hdr->file = file;
    new_hdr->line = line;
    
    // Update footer canary
    ptk_alloc_footer_t *new_footer = ptk_get_footer(new_hdr);
    new_footer->footer_canary = PTK_ALLOC_FOOTER_CANARY;
    
    debug("ptk_realloc: reallocated to %zu bytes at %p (user: %p) from %s:%d", new_total_size, raw, new_user_ptr, file, line);
    ptk_set_err(PTK_OK);
    return new_user_ptr;
}

void ptk_free_impl(const char *file, int line, void **ptr_ref) {
    if(!ptr_ref) {
        warn("ptk_free: called with null pointer reference at %s:%d", file, line);
        return;
    }
    void *ptr = *ptr_ref;
    if(!ptr) {
        debug("ptk_free: called with null pointer at %s:%d", file, line);
        return;
    }
    
    // Validate canaries before proceeding with free
    if(!ptk_validate_canaries(ptr)) {
        error("ptk_free: canary validation failed for pointer %p at %s:%d", ptr, file, line);
        error("ptk_free: refusing to free potentially invalid pointer");
        // Do not set ptk_set_err here as ptk_validate_canaries already did
        return;
    }
    
    ptk_alloc_header_t *hdr = ptk_get_header(ptr);
    debug("ptk_free: freeing memory at %p (user: %p) allocated from %s:%d", 
          hdr, ptr, hdr->file, hdr->line);
    
    // Call destructor if present
    if(hdr->destructor) {
        debug("ptk_free: calling destructor for memory at %p", ptr);
        hdr->destructor(ptr);
    }
    
    // Clear canaries to detect double-free
    hdr->header_canary = 0xDEADDEADDEADDEADULL;  // Invalid canary
    ptk_alloc_footer_t *footer = ptk_get_footer(hdr);
    footer->footer_canary = 0xDEADDEADDEADDEADULL;  // Invalid canary
    
    // Free the raw allocation
    free(hdr);
    *ptr_ref = NULL;
    ptk_set_err(PTK_OK);
}
