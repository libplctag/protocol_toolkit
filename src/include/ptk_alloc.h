#pragma once

/**
 * @file ptk_alloc.h
 * @brief Parent-Child Memory Allocation System for Protocol Toolkit
 *
 * This module provides a hierarchical memory management system where allocations
 * can be organized into parent-child relationships. This enables automatic cleanup
 * of related resources and provides robust debug tracking.
 *
 * ## Key Features
 *
 * - **Parent-Child Relationships**: Allocations can be organized hierarchically
 * - **Automatic Cleanup**: Freeing a parent automatically frees all its children
 * - **LIFO Destruction**: Children are freed in reverse order of addition
 * - **Debug Tracking**: All allocations track file/line information
 * - **Destructors**: Optional cleanup functions for custom resource management
 * - **Minimal Overhead**: 44 bytes per allocation for metadata
 * - **Memory Alignment**: All allocations are rounded up to 16-byte alignment
 *
 * ## Basic Usage
 *
 * ```c
 * // Create a parent allocation
 * void *parent = ptk_alloc(NULL, 1024, NULL);
 *
 * // Create children - they will be automatically freed when parent is freed
 * void *child1 = ptk_alloc(parent, 256, NULL);
 * void *child2 = ptk_alloc(parent, 512, my_destructor);
 *
 * // Add an existing allocation as a child
 * void *existing = ptk_alloc(NULL, 128, NULL);
 * ptk_add_child(parent, existing);
 *
 * // Free parent - automatically frees child1, child2, and existing
 * ptk_free(parent);
 * ```
 *
 * ## Parent-Child Semantics
 *
 * - **Parent Allocation**: Created with `ptk_alloc(NULL, size, destructor)`
 * - **Child Allocation**: Created with `ptk_alloc(parent, size, destructor)`
 * - **Adding Children**: Use `ptk_add_child(parent, child)` to adopt existing allocations
 * - **Freeing Parents**: Automatically frees all children in LIFO order
 * - **Freeing Children**: Calling `ptk_free()` on a child has no effect (safe but ignored)
 * - **Destructors**: Called in LIFO order during cleanup (children first, then parent)
 *
 * ## Complex Hierarchies
 *
 * You can create complex parent-child relationships, including adding parents
 * as children of other parents:
 *
 * ```c
 * // Create two separate hierarchies
 * void *parent1 = ptk_alloc(NULL, 1024, NULL);
 * void *child1a = ptk_alloc(parent1, 256, NULL);
 * void *child1b = ptk_alloc(parent1, 256, NULL);
 *
 * void *parent2 = ptk_alloc(NULL, 2048, NULL);
 * void *child2a = ptk_alloc(parent2, 512, NULL);
 * void *child2b = ptk_alloc(parent2, 512, NULL);
 *
 * // Add parent2 as a child of parent1
 * ptk_add_child(parent1, parent2);
 *
 * // Now freeing parent1 will free: child1b, child1a, child2b, child2a, parent2
 * ptk_free(parent1);
 * ```
 *
 * ## Safety Considerations
 *
 * - **Cycles**: Avoid creating cycles in parent-child relationships
 * - **Double-Free**: Safe - attempting to free a child is ignored
 * - **Use-After-Free**: Be careful with pointers after parent is freed
 * - **Reallocation**: `ptk_realloc()` on children may leave stale parent references
 *
 * ## Debug Information
 *
 * All allocations automatically track:
 * - File name and line number where allocated
 * - Size of allocation
 * - Parent-child relationships
 * - Destructor functions
 *
 * Debug information is logged during allocation and deallocation when debug
 * logging is enabled.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "ptk_err.h"

/**
 * @brief Allocate memory with optional parent and destructor
 *
 * Creates a new memory allocation. If a parent is provided, the allocation
 * becomes a child of that parent and will be automatically freed when the
 * parent is freed.
 *
 * @param parent Parent allocation (NULL for root allocation)
 * @param size Size of memory to allocate in bytes
 * @param destructor Optional cleanup function called during deallocation
 * @return Pointer to allocated memory, or NULL on failure
 *
 * @note This is a macro that automatically captures file/line information
 * @note Children are added to parent's child chain at the end (tail insertion)
 * @note If parent is provided and ptk_add_child() fails, allocation is freed
 *
 * @example
 * ```c
 * // Create parent allocation
 * void *parent = ptk_alloc(NULL, 1024, NULL);
 *
 * // Create child allocation
 * void *child = ptk_alloc(parent, 256, my_destructor);
 * ```
 */
#define ptk_alloc(parent, size, destructor) \
    ptk_alloc_impl(__FILE__, __LINE__, parent, size, destructor)
extern void *ptk_alloc_impl(const char *file, int line, void *parent, size_t size, void (*destructor)(void *ptr));

/**
 * @brief Resize an allocated memory block
 *
 * Changes the size of an existing allocation. The allocation retains its
 * parent-child relationships and other metadata.
 *
 * This fails if called on a NULL pointer or if the new size is 0 or if the memory is
 * already marked as free.
 *
 * @param ptr Pointer to memory block to resize
 * @param new_size New size in bytes
 * @return Pointer to resized memory block, or NULL on failure
 *
 * @note This is a macro that automatically captures file/line information
 * @note Contents are preserved up to the minimum of old and new sizes
 * @note If ptr is NULL, behaves like ptk_alloc(NULL, new_size, NULL)
 * @note If new_size is 0, behaves like ptk_free(ptr) and returns NULL
 * @note Reallocation may change the memory address
 * @warning Reallocating child allocations may leave stale parent references
 *
 * @example
 * ```c
 * void *ptr = ptk_alloc(NULL, 1024, NULL);
 * ptr = ptk_realloc(ptr, 2048);  // Resize to 2048 bytes
 * ```
 */
#define ptk_realloc(ptr, new_size) \
    ptk_realloc_impl(__FILE__, __LINE__, ptr, new_size)
extern void *ptk_realloc_impl(const char *file, int line, void *ptr, size_t new_size);

/**
 * @brief Add an existing allocation as a child of another allocation
 *
 * Adopts an existing allocation as a child. The child will be automatically
 * freed when the parent is freed. This is useful for transferring ownership
 * of allocations or building complex hierarchies.
 *
 * @param parent Parent allocation to adopt the child
 * @param child Child allocation to be adopted
 * @return PTK_OK on success, error code on failure
 *
 * @note This is a macro that automatically captures file/line information
 * @note Child is added at the end of the parent's child chain (tail insertion)
 * @note The child must not already be a child of another allocation
 * @note Both parent and child must be valid allocations from this allocator
 * @note Returns PTK_ERR_INVALID_PARAM if child is already adopted by another parent
 *
 * @example
 * ```c
 * void *parent = ptk_alloc(NULL, 1024, NULL);
 * void *child = ptk_alloc(NULL, 256, NULL);  // Independent allocation
 * ptk_add_child(parent, child);              // Now child belongs to parent
 * ```
 */
#define ptk_add_child(parent, child) \
    ptk_add_child_impl(__FILE__, __LINE__, parent, child)
extern ptk_err ptk_add_child_impl(const char *file, int line, void *parent, void *child);

/**
 * @brief Free an allocated memory block and all its children
 *
 * Frees a memory block and all of its children in LIFO order (reverse order
 * of addition). Destructors are called for each allocation before freeing.
 *
 * @param ptr Pointer to memory block to free
 *
 * @note This is a macro that automatically captures file/line information
 * @note If ptr is a child allocation, a warning is logged and the call returns early
 * @note Children are freed in LIFO order (last added, first freed)
 * @note Destructors are called before freeing each allocation
 * @note It is safe to call ptk_free(NULL)
 * @warning Attempting to free a child allocation will log a warning and be ignored
 * @warning After a parent is freed, child pointers become invalid
 * @warning Calling ptk_free() on a child after parent is freed is undefined behavior
 *
 * If the parent is A->B->C and the child is D->E->F->G and you call ptk_add_child(A, D), the
 * child will be added to the parent's child list. The result will be A->B->C->D->E->F->G.
 * If you then free A, all of its children (B, C, D, E, F, G) will be freed as well.
 *
 * It is an error to add a new child chain to a parent that already contains one or more of the children.
 *
 * @example
 * ```c
 * void *parent = ptk_alloc(NULL, 1024, NULL);
 * void *child1 = ptk_alloc(parent, 256, NULL);
 * void *child2 = ptk_alloc(parent, 256, NULL);
 *
 * ptk_free(child1);  // Safe no-op - child1 remains allocated
 * ptk_free(parent);  // Frees child2, then child1, then parent
 * // child1 and child2 pointers are now invalid
 * // ptk_free(child1);  // UNDEFINED BEHAVIOR - don't do this!
 * ```
 */
#define ptk_free(ptr) \
    ptk_free_impl(__FILE__, __LINE__, ptr)
extern void ptk_free_impl(const char *file, int line, void *ptr);
