#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <ptk_err.h>

/**
 * @brief Allocate memory with optional destructor
 *
 * Creates a new memory allocation. If a destructor is provided, it will be called
 * when the allocation is freed.
 *
 * @param size Size of memory to allocate in bytes
 * @param destructor Optional cleanup function called during deallocation
 * @return Pointer to allocated memory, or NULL on failure
 *
 * @note This is a macro that automatically captures file/line information
 *
 * @example
 * ```c
 * // Create an allocation
 * void *obj = ptk_alloc(1024, NULL);
 *
 * // free it later, calls the destructor if provided
 * ptk_free(obj);
 * ```
 */
#define ptk_alloc(size, destructor) \
    ptk_alloc_impl(__FILE__, __LINE__, size, destructor)
extern void *ptk_alloc_impl(const char *file, int line, size_t size, void (*destructor)(void *ptr));

/**
 * @brief Resize an allocated memory block
 *
 * Changes the size of an existing allocation.
 *
 * This fails if called on a NULL pointer or if the new size is 0 or if the memory is
 * already marked as free.  Failure to reallocate will return NULL and log an error.
 *
 * If the new size is larger than the current size, the contents are preserved up to the minimum of old and new sizes.
 * The new memory is zeroed out if the new size is larger than the old size.
 *
 * @param ptr Pointer to memory block to resize
 * @param new_size New size in bytes
 * @return Pointer to resized memory block, or NULL on failure
 *
 * @note This is a macro that automatically captures file/line information
 * @note Contents are preserved up to the minimum of old and new sizes
 * @note Reallocation may change the memory address
 *
 * @example
 * ```c
 * void *ptr = ptk_alloc(1024, NULL);
 * ptr = ptk_realloc(ptr, 2048);  // Resize to 2048 bytes
 * ```
 */
#define ptk_realloc(ptr, new_size) \
    ptk_realloc_impl(__FILE__, __LINE__, ptr, new_size)
extern void *ptk_realloc_impl(const char *file, int line, void *ptr, size_t new_size);


/**
 * @brief Free an allocated memory block after calling its destructor
 *
 * Calls the destructor if provided, then frees the memory and sets the pointer to NULL.
 *
 * @param ptr_ref Pointer to pointer to memory block to free
 *
 * @note This is a macro that automatically captures file/line information
 * @note Destructors are called before freeing each allocation
 * @note It is safe to call ptk_free(NULL) or ptk_free(&null_ptr)
 * @note The pointer will be set to NULL after freeing to prevent use-after-free bugs
 *
 * @example
 * ```c
 * void *ptr = ptk_alloc(1024, NULL);
 * ptk_free(&ptr);  // ptr is now NULL
 * ```
 */
#define ptk_free(ptr_ref) \
    ptk_free_impl(__FILE__, __LINE__, (void **)(ptr_ref))
extern void ptk_free_impl(const char *file, int line, void **ptr_ref);
