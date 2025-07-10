#include "ptk_alloc.h"
#include "ptk_log.h"
#include <stdlib.h>
#include <string.h>


//=============================================================================
// ALLOCATION HEADER
//=============================================================================


// Internal allocation header - 44 bytes total on 64-bit systems.
typedef struct ptk_alloc_header {
    struct ptk_alloc_header *parent;  // Parent allocation (8 bytes)
    struct ptk_alloc_header *child;   // First child of this allocation (8 bytes)
    void (*destructor)(void *ptr);    // Destructor function (8 bytes)
    const char *alloc_file;           // File where allocated (8 bytes)
    size_t size;                      // Size of allocation (8 bytes)
    uint32_t line;                    // Line number and free bit
} ptk_alloc_header_t;

// Alignment helper
#define PTK_ALIGN_SIZE(size, alignment) (((size) + (alignment) - 1) & ~((alignment) - 1))
#define PTK_DEFAULT_ALIGNMENT 16

//=============================================================================
// HEADER UTILITIES
//=============================================================================

// Get header from user pointer
static inline ptk_alloc_header_t *get_header(void *ptr) {
    if(!ptr) { return NULL; }
    return (ptk_alloc_header_t *)((char *)ptr - PTK_ALIGN_SIZE(sizeof(ptk_alloc_header_t), PTK_DEFAULT_ALIGNMENT));
}

// Get user pointer from header
static inline void *get_user_ptr(ptk_alloc_header_t *header) {
    if(!header) { return NULL; }
    return (char *)header + PTK_ALIGN_SIZE(sizeof(ptk_alloc_header_t), PTK_DEFAULT_ALIGNMENT);
}

static inline const char *get_alloc_file(ptk_alloc_header_t *header) { return (header ? header->alloc_file : "unknown"); }

// Extract line number from line
static inline uint32_t get_line_number(ptk_alloc_header_t *header) { return (header ? header->line : 0); }

//=============================================================================
// CORE ALLOCATION FUNCTIONS
//=============================================================================

void *ptk_alloc_impl(const char *file, int line, void *parent, size_t size, void (*destructor)(void *ptr)) {
    info("Starting with size %zu", size);

    if(size == 0) {
        warn("called with zero size!");
        return NULL;
    }

    // Align size to default alignment
    size_t aligned_size = PTK_ALIGN_SIZE(size, PTK_DEFAULT_ALIGNMENT);

    // Allocate memory: header + user data
    size_t total_size = PTK_ALIGN_SIZE(sizeof(ptk_alloc_header_t), PTK_DEFAULT_ALIGNMENT) + aligned_size;
    ptk_alloc_header_t *header = (ptk_alloc_header_t *)malloc(total_size);

    if(!header) {
        error("Failed to allocate %zu bytes at %s:%d", total_size, file ? file : "unknown", line);
        return NULL;
    }

    // Initialize header
    header->destructor = destructor;
    header->alloc_file = file;
    header->size = aligned_size;
    header->line = (uint32_t)line;
    header->parent = NULL;  // if there is a parent, it will be set later
    header->child = NULL;   // No children initially

    void *user_ptr = get_user_ptr(header);

    // If parent provided, add as child
    if(parent) {
        ptk_err err = ptk_add_child_impl(file, line, parent, user_ptr);
        if(err != PTK_OK) {
            error("Failed to add child allocation at %s:%d", file ? file : "unknown", line);
            free(header);
            return NULL;
        }
    }

    trace("Allocated %zu bytes at %p (parent: %p) at %s:%d", aligned_size, user_ptr, parent, file ? file : "unknown", line);

    return user_ptr;
}

void *ptk_realloc_impl(const char *file, int line, void *ptr, size_t new_size) {
    info("Called from %s:%d with new size %zu", file ? file : "unknown", line, new_size);

    // no realloc of a null pointer
    if(!ptr) {
        warn("Called with null pointer!");
        return NULL;
    }

    // no realloc as free.
    if(new_size == 0) {
        warn("Called with zero size!");
        return NULL;
    }

    ptk_alloc_header_t *header = get_header(ptr);
    if(!header) {
        error("ptk_realloc called on invalid pointer %p", ptr);
        return NULL;
    }

    // Align new size
    size_t aligned_new_size = PTK_ALIGN_SIZE(new_size, PTK_DEFAULT_ALIGNMENT);

    // If new size is same as current, no change needed
    if(aligned_new_size == header->size) {
        trace("ptk_realloc: size unchanged (%zu bytes) for %p", aligned_new_size, ptr);
        return ptr;
    }

    // Allocate new memory
    size_t total_size = PTK_ALIGN_SIZE(sizeof(ptk_alloc_header_t), PTK_DEFAULT_ALIGNMENT) + aligned_new_size;
    ptk_alloc_header_t *new_header = (ptk_alloc_header_t *)realloc(header, total_size);

    if(!new_header) {
        error("Failed to reallocate %p to %zu bytes", ptr, aligned_new_size);
        return NULL;
    }

    if(new_header == header) {
        // If realloc didn't change the pointer, just return the original
        trace("Reallocation at %s:%d: pointer unchanged (%p) for %zu bytes", file ? file : "unknown", line, ptr,
              aligned_new_size);
        return ptr;
    }

    // update the size to the new aligned size
    new_header->size = aligned_new_size;

    trace("Reallocation at %s:%d: successful with new pointer %p for %zu bytes", file ? file : "unknown", line,
          get_user_ptr(new_header), aligned_new_size);

    // if the allocation is a child, we need to update the parent's child list
    if(new_header->parent) { new_header->parent->child = new_header; }

    /* update the child's parent to point to this new header */
    if(new_header->child) { new_header->child->parent = new_header; }

    return get_user_ptr(new_header);
}


/**
 * @brief Add a new child to an existing parent.
 *
 * If the child has its own children, it will splice the entire chain
 * at the end of the parent's child list.
 *
 * This is illegal if the child is not a parent!
 *
 * If the parent is A->B->C and the child is D->E, then the result will be:
 * A->B->C->D->E
 *
 * @param file
 * @param line
 * @param parent
 * @param child
 * @return ptk_err
 */
ptk_err ptk_add_child_impl(const char *file, int line, void *parent, void *child) {
    info("Called from %s:%d to add child %p to parent %p", file ? file : "unknown", line, child, parent);

    if(!parent || !child) { return PTK_ERR_NULL_PTR; }

    ptk_alloc_header_t *parent_header = get_header(parent);
    ptk_alloc_header_t *child_header = get_header(child);

    if(!parent_header || !child_header) {
        error("ptk_add_child called with invalid pointers (parent: %p, child: %p)", parent, child);
        return PTK_ERR_NULL_PTR;
    }

    if(child_header->parent) {
        warn("Child allocation %p is already marked as a child, called from %s:%d", child_header, file ? file : "unknown", line);
        return PTK_ERR_INVALID_PARAM;  // Child is already a child
    }

    // walk down the parent chain to find the last child
    ptk_alloc_header_t *last_child_header = parent_header;
    while(last_child_header && last_child_header->child) { last_child_header = last_child_header->child; }

    if(last_child_header) {
        trace("Found last child %p in parent's chain", last_child_header);
        last_child_header->child = child_header;
        child_header->parent = parent_header;
    } else {
        warn("This parent itself is NULL?!");
        return PTK_ERR_INVALID_PARAM;
    }

    return PTK_OK;
}


void ptk_free_impl(const char *file, int line, void *ptr) {
    info("Called from %s:%d to free %p", file ? file : "unknown", line, ptr);

    if(!ptr) { return; }

    ptk_alloc_header_t *header = get_header(ptr);
    if(!header) {
        warn("ptk_free called on invalid pointer %p", ptr);
        return;
    }

    if(header->parent) {
        // This is an error, as it is setting up a double free!

        ptk_alloc_header_t *parent = header->parent;

        // chain up to find the top parent
        while(parent && parent->parent) { parent = parent->parent; }

        warn("Attempt to free a child allocation %p with top parent %p at %s:%d", ptr, parent, file ? file : "unknown", line);

        return;
    }

    // walk down the parent chain to find the last child
    ptk_alloc_header_t *last_child_header = header;
    while(last_child_header && last_child_header->child) { last_child_header = last_child_header->child; }

    // start at the end of the chain and work backwards
    while(last_child_header) {
        ptk_alloc_header_t *parent = last_child_header->parent;

        if(last_child_header->destructor) {
            debug("Called from %s:%d, calling destructor for child %p allocated at %s:%d", (file ? file : "unknown"), line,
                  get_user_ptr(last_child_header), get_alloc_file(last_child_header), get_line_number(last_child_header));

            if(last_child_header->destructor) { last_child_header->destructor(get_user_ptr(last_child_header)); }

            last_child_header->child = NULL;  // Clear child pointer to avoid double free

            free(last_child_header);
        }

        // free the memory for this node.
        free(last_child_header);

        last_child_header = parent;
    }

    free(header);
}
