#pragma once


#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "ptk_err.h"

/**
 * @brief Wrappers around platform-specific atomic operations.
 */

//=============================================================================
// ATOMICS
//=============================================================================

/* need #ifdefs here for platform-specific definitions */
#define ptk_atomic volatile

ptk_err ptk_atomic_load_u8(uint8_t *dest_value, ptk_atomic uint8_t *src_value);
ptk_err ptk_atomic_store_u8(ptk_atomic uint8_t *dest_value, uint8_t src_value);
ptk_err ptk_atomic_fetch_add_u8(ptk_atomic uint8_t *dest_value, uint8_t src_value);
ptk_err ptk_atomic_add_fetch_u8(ptk_atomic uint8_t *dest_value, uint8_t src_value);
ptk_err ptk_atomic_fetch_sub_u8(ptk_atomic uint8_t *dest_value, uint8_t src_value);
ptk_err ptk_atomic_sub_fetch_u8(ptk_atomic uint8_t *dest_value, uint8_t src_value);
ptk_err ptk_atomic_fetch_and_u8(ptk_atomic uint8_t *dest_value, uint8_t src_value);
ptk_err ptk_atomic_and_fetch_u8(ptk_atomic uint8_t *dest_value, uint8_t src_value);
ptk_err ptk_atomic_fetch_or_u8(ptk_atomic uint8_t *dest_value, uint8_t src_value);
ptk_err ptk_atomic_or_fetch_u8(ptk_atomic uint8_t *dest_value, uint8_t src_value);
ptk_err ptk_atomic_fetch_xor_u8(ptk_atomic uint8_t *dest_value, uint8_t src_value);
ptk_err ptk_atomic_xor_fetch_u8(ptk_atomic uint8_t *dest_value, uint8_t src_value);
ptk_err ptk_atomic_compare_and_swap_u8(ptk_atomic uint8_t *dest_value, uint8_t old_value, uint8_t new_value);

ptk_err ptk_atomic_load_u16(uint16_t *dest_value, ptk_atomic uint16_t *src_value);
ptk_err ptk_atomic_store_u16(ptk_atomic uint16_t *dest_value, uint16_t src_value);
ptk_err ptk_atomic_fetch_add_u16(ptk_atomic uint16_t *dest_value, uint16_t src_value);
ptk_err ptk_atomic_add_fetch_u16(ptk_atomic uint16_t *dest_value, uint16_t src_value);
ptk_err ptk_atomic_fetch_sub_u16(ptk_atomic uint16_t *dest_value, uint16_t src_value);
ptk_err ptk_atomic_sub_fetch_u16(ptk_atomic uint16_t *dest_value, uint16_t src_value);
ptk_err ptk_atomic_fetch_and_u16(ptk_atomic uint16_t *dest_value, uint16_t src_value);
ptk_err ptk_atomic_and_fetch_u16(ptk_atomic uint16_t *dest_value, uint16_t src_value);
ptk_err ptk_atomic_fetch_or_u16(ptk_atomic uint16_t *dest_value, uint16_t src_value);
ptk_err ptk_atomic_or_fetch_u16(ptk_atomic uint16_t *dest_value, uint16_t src_value);
ptk_err ptk_atomic_fetch_xor_u16(ptk_atomic uint16_t *dest_value, uint16_t src_value);
ptk_err ptk_atomic_xor_fetch_u16(ptk_atomic uint16_t *dest_value, uint16_t src_value);
ptk_err ptk_atomic_compare_and_swap_u16(ptk_atomic uint16_t *dest_value, uint16_t old_value, uint16_t new_value);

ptk_err ptk_atomic_load_u32(uint32_t *dest_value, ptk_atomic uint32_t *src_value);
ptk_err ptk_atomic_store_u32(ptk_atomic uint32_t *dest_value, uint32_t src_value);
ptk_err ptk_atomic_fetch_add_u32(ptk_atomic uint32_t *dest_value, uint32_t src_value);
ptk_err ptk_atomic_add_fetch_u32(ptk_atomic uint32_t *dest_value, uint32_t src_value);
ptk_err ptk_atomic_fetch_sub_u32(ptk_atomic uint32_t *dest_value, uint32_t src_value);
ptk_err ptk_atomic_sub_fetch_u32(ptk_atomic uint32_t *dest_value, uint32_t src_value);
ptk_err ptk_atomic_fetch_and_u32(ptk_atomic uint32_t *dest_value, uint32_t src_value);
ptk_err ptk_atomic_and_fetch_u32(ptk_atomic uint32_t *dest_value, uint32_t src_value);
ptk_err ptk_atomic_fetch_or_u32(ptk_atomic uint32_t *dest_value, uint32_t src_value);
ptk_err ptk_atomic_or_fetch_u32(ptk_atomic uint32_t *dest_value, uint32_t src_value);
ptk_err ptk_atomic_fetch_xor_u32(ptk_atomic uint32_t *dest_value, uint32_t src_value);
ptk_err ptk_atomic_xor_fetch_u32(ptk_atomic uint32_t *dest_value, uint32_t src_value);
ptk_err ptk_atomic_compare_and_swap_u32(ptk_atomic uint32_t *dest_value, uint32_t old_value, uint32_t new_value);

ptk_err ptk_atomic_load_u64(uint64_t *dest_value, ptk_atomic uint64_t *src_value);
ptk_err ptk_atomic_store_u64(ptk_atomic uint64_t *dest_value, uint64_t src_value);
ptk_err ptk_atomic_fetch_add_u64(ptk_atomic uint64_t *dest_value, uint64_t src_value);
ptk_err ptk_atomic_add_fetch_u64(ptk_atomic uint64_t *dest_value, uint64_t src_value);
ptk_err ptk_atomic_fetch_sub_u64(ptk_atomic uint64_t *dest_value, uint64_t src_value);
ptk_err ptk_atomic_sub_fetch_u64(ptk_atomic uint64_t *dest_value, uint64_t src_value);
ptk_err ptk_atomic_fetch_and_u64(ptk_atomic uint64_t *dest_value, uint64_t src_value);
ptk_err ptk_atomic_and_fetch_u64(ptk_atomic uint64_t *dest_value, uint64_t src_value);
ptk_err ptk_atomic_fetch_or_u64(ptk_atomic uint64_t *dest_value, uint64_t src_value);
ptk_err ptk_atomic_or_fetch_u64(ptk_atomic uint64_t *dest_value, uint64_t src_value);
ptk_err ptk_atomic_fetch_xor_u64(ptk_atomic uint64_t *dest_value, uint64_t src_value);
ptk_err ptk_atomic_xor_fetch_u64(ptk_atomic uint64_t *dest_value, uint64_t src_value);
ptk_err ptk_atomic_compare_and_swap_u64(ptk_atomic uint64_t *dest_value, uint64_t old_value, uint64_t new_value);

ptk_err ptk_atomic_load_ptr(void **dest_value, ptk_atomic void **src_value);
ptk_err ptk_atomic_store_ptr(ptk_atomic void **dest_value, void *src_value);
ptk_err ptk_atomic_compare_and_swap_ptr(ptk_atomic void **dest_value, void *old_value, void *new_value);
