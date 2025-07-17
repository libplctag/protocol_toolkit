
#pragma once

/**
 * @file ptk_atomic.h
 * @brief Platform-independent atomic operations for Protocol Toolkit
 *
 * This header provides wrappers for atomic operations on integer and pointer types.
 * All operations are implemented using platform-specific primitives for thread safety.
 *
 * Supported operations (for each type):
 *   - load, store
 *   - fetch_add, add_fetch
 *   - fetch_sub, sub_fetch
 *   - fetch_and, and_fetch
 *   - fetch_or, or_fetch
 *   - fetch_xor, xor_fetch
 *   - compare_and_swap
 *
 * Supported data types:
 *   - uint8_t, uint16_t, uint32_t, uint64_t
 *   - void* (pointer)
 *
 * Example usage:
 *   uint32_t val = 0;
 *   ptk_atomic_fetch_add_u32(&val, 1);
 *   ptk_atomic_compare_and_swap_u32(&val, old, new);
 *
 * See function documentation for details.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <ptk_defs.h>
#include <ptk_err.h>

//=============================================================================
// ATOMICS
//=============================================================================

/* need #ifdefs here for platform-specific definitions */
#define ptk_atomic volatile


PTK_API uint8_t ptk_atomic_load_u8(ptk_atomic uint8_t *src_value);
PTK_API ptk_err_t ptk_atomic_store_u8(ptk_atomic uint8_t *dest_value, uint8_t src_value);
PTK_API uint8_t ptk_atomic_fetch_add_u8(ptk_atomic uint8_t *dest_value, uint8_t src_value);
PTK_API uint8_t ptk_atomic_add_fetch_u8(ptk_atomic uint8_t *dest_value, uint8_t src_value);
PTK_API uint8_t ptk_atomic_fetch_sub_u8(ptk_atomic uint8_t *dest_value, uint8_t src_value);
PTK_API uint8_t ptk_atomic_sub_fetch_u8(ptk_atomic uint8_t *dest_value, uint8_t src_value);
PTK_API uint8_t ptk_atomic_fetch_and_u8(ptk_atomic uint8_t *dest_value, uint8_t src_value);
PTK_API uint8_t ptk_atomic_and_fetch_u8(ptk_atomic uint8_t *dest_value, uint8_t src_value);
PTK_API uint8_t ptk_atomic_fetch_or_u8(ptk_atomic uint8_t *dest_value, uint8_t src_value);
PTK_API uint8_t ptk_atomic_or_fetch_u8(ptk_atomic uint8_t *dest_value, uint8_t src_value);
PTK_API uint8_t ptk_atomic_fetch_xor_u8(ptk_atomic uint8_t *dest_value, uint8_t src_value);
PTK_API uint8_t ptk_atomic_xor_fetch_u8(ptk_atomic uint8_t *dest_value, uint8_t src_value);
PTK_API uint8_t ptk_atomic_compare_and_swap_u8(ptk_atomic uint8_t *dest_value, uint8_t expected_old_value, uint8_t new_value);


PTK_API uint16_t ptk_atomic_load_u16(ptk_atomic uint16_t *src_value);
PTK_API ptk_err_t ptk_atomic_store_u16(ptk_atomic uint16_t *dest_value, uint16_t src_value);
PTK_API uint16_t ptk_atomic_fetch_add_u16(ptk_atomic uint16_t *dest_value, uint16_t src_value);
PTK_API uint16_t ptk_atomic_add_fetch_u16(ptk_atomic uint16_t *dest_value, uint16_t src_value);
PTK_API uint16_t ptk_atomic_fetch_sub_u16(ptk_atomic uint16_t *dest_value, uint16_t src_value);
PTK_API uint16_t ptk_atomic_sub_fetch_u16(ptk_atomic uint16_t *dest_value, uint16_t src_value);
PTK_API uint16_t ptk_atomic_fetch_and_u16(ptk_atomic uint16_t *dest_value, uint16_t src_value);
PTK_API uint16_t ptk_atomic_and_fetch_u16(ptk_atomic uint16_t *dest_value, uint16_t src_value);
PTK_API uint16_t ptk_atomic_fetch_or_u16(ptk_atomic uint16_t *dest_value, uint16_t src_value);
PTK_API uint16_t ptk_atomic_or_fetch_u16(ptk_atomic uint16_t *dest_value, uint16_t src_value);
PTK_API uint16_t ptk_atomic_fetch_xor_u16(ptk_atomic uint16_t *dest_value, uint16_t src_value);
PTK_API uint16_t ptk_atomic_xor_fetch_u16(ptk_atomic uint16_t *dest_value, uint16_t src_value);
PTK_API uint16_t ptk_atomic_compare_and_swap_u16(ptk_atomic uint16_t *dest_value, uint16_t expected_old_value, uint16_t new_value);


PTK_API uint32_t ptk_atomic_load_u32(ptk_atomic uint32_t *src_value);
PTK_API ptk_err_t ptk_atomic_store_u32(ptk_atomic uint32_t *dest_value, uint32_t src_value);
PTK_API uint32_t ptk_atomic_fetch_add_u32(ptk_atomic uint32_t *dest_value, uint32_t src_value);
PTK_API uint32_t ptk_atomic_add_fetch_u32(ptk_atomic uint32_t *dest_value, uint32_t src_value);
PTK_API uint32_t ptk_atomic_fetch_sub_u32(ptk_atomic uint32_t *dest_value, uint32_t src_value);
PTK_API uint32_t ptk_atomic_sub_fetch_u32(ptk_atomic uint32_t *dest_value, uint32_t src_value);
PTK_API uint32_t ptk_atomic_fetch_and_u32(ptk_atomic uint32_t *dest_value, uint32_t src_value);
PTK_API uint32_t ptk_atomic_and_fetch_u32(ptk_atomic uint32_t *dest_value, uint32_t src_value);
PTK_API uint32_t ptk_atomic_fetch_or_u32(ptk_atomic uint32_t *dest_value, uint32_t src_value);
PTK_API uint32_t ptk_atomic_or_fetch_u32(ptk_atomic uint32_t *dest_value, uint32_t src_value);
PTK_API uint32_t ptk_atomic_fetch_xor_u32(ptk_atomic uint32_t *dest_value, uint32_t src_value);
PTK_API uint32_t ptk_atomic_xor_fetch_u32(ptk_atomic uint32_t *dest_value, uint32_t src_value);
PTK_API uint32_t ptk_atomic_compare_and_swap_u32(ptk_atomic uint32_t *dest_value, uint32_t expected_old_value, uint32_t new_value);


PTK_API uint64_t ptk_atomic_load_u64(ptk_atomic uint64_t *src_value);
PTK_API ptk_err_t ptk_atomic_store_u64(ptk_atomic uint64_t *dest_value, uint64_t src_value);
PTK_API uint64_t ptk_atomic_fetch_add_u64(ptk_atomic uint64_t *dest_value, uint64_t src_value);
PTK_API uint64_t ptk_atomic_add_fetch_u64(ptk_atomic uint64_t *dest_value, uint64_t src_value);
PTK_API uint64_t ptk_atomic_fetch_sub_u64(ptk_atomic uint64_t *dest_value, uint64_t src_value);
PTK_API uint64_t ptk_atomic_sub_fetch_u64(ptk_atomic uint64_t *dest_value, uint64_t src_value);
PTK_API uint64_t ptk_atomic_fetch_and_u64(ptk_atomic uint64_t *dest_value, uint64_t src_value);
PTK_API uint64_t ptk_atomic_and_fetch_u64(ptk_atomic uint64_t *dest_value, uint64_t src_value);
PTK_API uint64_t ptk_atomic_fetch_or_u64(ptk_atomic uint64_t *dest_value, uint64_t src_value);
PTK_API uint64_t ptk_atomic_or_fetch_u64(ptk_atomic uint64_t *dest_value, uint64_t src_value);
PTK_API uint64_t ptk_atomic_fetch_xor_u64(ptk_atomic uint64_t *dest_value, uint64_t src_value);
PTK_API uint64_t ptk_atomic_xor_fetch_u64(ptk_atomic uint64_t *dest_value, uint64_t src_value);
PTK_API uint64_t ptk_atomic_compare_and_swap_u64(ptk_atomic uint64_t *dest_value, uint64_t expected_old_value, uint64_t new_value);

PTK_API void *ptk_atomic_load_ptr(ptk_atomic void **src_value);
PTK_API ptk_err_t ptk_atomic_store_ptr(ptk_atomic void **dest_value, void *src_value);
PTK_API void *ptk_atomic_compare_and_swap_ptr(ptk_atomic void **dest_value, void *expected_old_value, void *new_value);
