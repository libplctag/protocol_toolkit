/**
 * @file test_ptk_atomic.c
 * @brief Tests for ptk_atomic API
 *
 * This file tests atomic operations. Logging uses ptk_log.h, not ptk_atomic.h except for the functions under test.
 */
#include <ptk_atomic.h>
#include <ptk_log.h>
#include <stdio.h>

/*
 * TODO:
 * - Add more tests for edge cases (e.g., unaligned addresses)
 * - Test with different data types (e.g., int, float)
 * - Test all atomic functions:
 *   - ptk_atomic_fetch_add_u8
 *   - ptk_atomic_add_fetch_u8
 *   - ptk_atomic_fetch_sub_u8
 *   - ptk_atomic_sub_fetch_u8
 *   - ptk_atomic_fetch_and_u8
 *   - ptk_atomic_and_fetch_u8
 *   - ptk_atomic_fetch_or_u8
 *   - ptk_atomic_or_fetch_u8
 *   - ptk_atomic_fetch_xor_u8
 *   - ptk_atomic_xor_fetch_u8
 *   - ptk_atomic_compare_and_swap_u8
 *   - ptk_atomic_load_u16
 *   - ptk_atomic_store_u16
 *   - ptk_atomic_fetch_add_u16
 *   - ptk_atomic_add_fetch_u16
 *   - ptk_atomic_fetch_sub_u16
 *   - ptk_atomic_sub_fetch_u16
 *   - ptk_atomic_fetch_and_u16
 *   - ptk_atomic_and_fetch_u16
 *   - ptk_atomic_fetch_or_u16
 *   - ptk_atomic_or_fetch_u16
 *   - ptk_atomic_fetch_xor_u16
 *   - ptk_atomic_xor_fetch_u16
 *   - ptk_atomic_compare_and_swap_u16
 *   - ptk_atomic_load_u32
 *   - ptk_atomic_store_u32
 *   - ptk_atomic_fetch_add_u32
 *   - ptk_atomic_add_fetch_u32
 *   - ptk_atomic_fetch_sub_u32
 *   - ptk_atomic_sub_fetch_u32
 *   - ptk_atomic_fetch_and_u32
 *   - ptk_atomic_and_fetch_u32
 *   - ptk_atomic_fetch_or_u32
 *   - ptk_atomic_or_fetch_u32
 *   - ptk_atomic_fetch_xor_u32
 *   - ptk_atomic_xor_fetch_u32
 *   - ptk_atomic_compare_and_swap_u32
 *   - ptk_atomic_load_u64
 *   - ptk_atomic_store_u64
 *   - ptk_atomic_fetch_add_u64
 *   - ptk_atomic_add_fetch_u64
 *   - ptk_atomic_fetch_sub_u64
 *   - ptk_atomic_sub_fetch_u64
 *   - ptk_atomic_fetch_and_u64
 *   - ptk_atomic_and_fetch_u64
 *   - ptk_atomic_fetch_or_u64
 *   - ptk_atomic_or_fetch_u64
 *   - ptk_atomic_fetch_xor_u64
 *   - ptk_atomic_xor_fetch_u64
 *   - ptk_atomic_compare_and_swap_u64
 *   - ptk_atomic_compare_and_swap_ptr
 */

/**
 * @brief Test atomic load/store operations
 * @return 0 on success, nonzero on failure
 */
int test_atomic_ops() {
    info("test_atomic_ops entry");
    uint8_t val = 42;
    uint8_t loaded = 0;
    if(ptk_atomic_store_u8(&val, 99) != PTK_OK) {
        error("atomic store failed");
        return 1;
    }
    if(ptk_atomic_load_u8(&loaded, &val) != PTK_OK) {
        error("atomic load failed");
        return 2;
    }
    if(loaded != 99) {
        error("atomic value mismatch");
        return 3;
    }
    info("test_atomic_ops exit");
    return 0;
}

int main() {
    int result = test_atomic_ops();
    if(result == 0) {
        info("ptk_atomic test PASSED");
    } else {
        error("ptk_atomic test FAILED");
    }
    return result;
}
