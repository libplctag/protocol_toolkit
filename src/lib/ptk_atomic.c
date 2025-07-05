#include <stdatomic.h>

#include "ptk_atomic.h"

#include "ptk_err.h"


#define PTK_ATOMIC_OP(type, op, func_suffix)                                   \
    ptk_err ptk_atomic_##op##_##func_suffix(ptk_atomic type *dest, type val) { \
        atomic_##type *ptr = (atomic_##type *)dest;                            \
        atomic_fetch_##op(ptr, val);                                           \
        return PTK_OK;                                                         \
    }

#define PTK_ATOMIC_OP_FETCH(type, op, func_suffix)                                   \
    ptk_err ptk_atomic_##op##_fetch_##func_suffix(ptk_atomic type *dest, type val) { \
        atomic_##type *ptr = (atomic_##type *)dest;                                  \
        atomic_##op##_fetch(ptr, val);                                               \
        return PTK_OK;                                                               \
    }

#define PTK_ATOMIC_CAS(type, func_suffix)                                                                  \
    ptk_err ptk_atomic_compare_and_swap_##func_suffix(ptk_atomic type *dest, type old_val, type new_val) { \
        atomic_##type *ptr = (atomic_##type *)dest;                                                        \
        atomic_compare_exchange_strong(ptr, &old_val, new_val);                                            \
        return PTK_OK;                                                                                     \
    }

#define PTK_ATOMIC_LOAD(type, func_suffix)                                    \
    ptk_err ptk_atomic_load_##func_suffix(type *dest, ptk_atomic type *src) { \
        *dest = atomic_load((atomic_##type *)src);                            \
        return PTK_OK;                                                        \
    }

#define PTK_ATOMIC_STORE(type, func_suffix)                                   \
    ptk_err ptk_atomic_store_##func_suffix(ptk_atomic type *dest, type src) { \
        atomic_store((atomic_##type *)dest, src);                             \
        return PTK_OK;                                                        \
    }

//=== u8
PTK_ATOMIC_LOAD(uint8_t, u8)
PTK_ATOMIC_STORE(uint8_t, u8)
PTK_ATOMIC_OP(uint8_t, fetch_add, u8)
PTK_ATOMIC_OP_FETCH(uint8_t, add, u8)
PTK_ATOMIC_OP(uint8_t, fetch_sub, u8)
PTK_ATOMIC_OP_FETCH(uint8_t, sub, u8)
PTK_ATOMIC_OP(uint8_t, fetch_and, u8)
PTK_ATOMIC_OP_FETCH(uint8_t, and, u8)
PTK_ATOMIC_OP(uint8_t, fetch_or, u8)
PTK_ATOMIC_OP_FETCH(uint8_t, or, u8)
PTK_ATOMIC_OP(uint8_t, fetch_xor, u8)
PTK_ATOMIC_OP_FETCH(uint8_t, xor, u8)
ptk_err ptk_atomic_compare_and_swap(ptk_atomic uint8_t *dest, uint8_t old, uint8_t new) {
    atomic_uint8_t *ptr = (atomic_uint8_t *)dest;
    atomic_compare_exchange_strong(ptr, &old, new);
    return PTK_OK;
}

//=== u16
PTK_ATOMIC_LOAD(uint16_t, u16)
PTK_ATOMIC_STORE(uint16_t, u16)
PTK_ATOMIC_OP(uint16_t, fetch_add, u16)
PTK_ATOMIC_OP_FETCH(uint16_t, add, u16)
PTK_ATOMIC_OP(uint16_t, fetch_sub, u16)
PTK_ATOMIC_OP_FETCH(uint16_t, sub, u16)
PTK_ATOMIC_OP(uint16_t, fetch_and, u16)
PTK_ATOMIC_OP_FETCH(uint16_t, and, u16)
PTK_ATOMIC_OP(uint16_t, fetch_or, u16)
PTK_ATOMIC_OP_FETCH(uint16_t, or, u16)
PTK_ATOMIC_OP(uint16_t, fetch_xor, u16)
PTK_ATOMIC_OP_FETCH(uint16_t, xor, u16)
PTK_ATOMIC_CAS(uint16_t, u16)

//=== u32
PTK_ATOMIC_LOAD(uint32_t, u32)
PTK_ATOMIC_STORE(uint32_t, u32)
PTK_ATOMIC_OP(uint32_t, fetch_add, u32)
PTK_ATOMIC_OP_FETCH(uint32_t, add, u32)
PTK_ATOMIC_OP(uint32_t, fetch_sub, u32)
PTK_ATOMIC_OP_FETCH(uint32_t, sub, u32)
PTK_ATOMIC_OP(uint32_t, fetch_and, u32)
PTK_ATOMIC_OP_FETCH(uint32_t, and, u32)
PTK_ATOMIC_OP(uint32_t, fetch_or, u32)
PTK_ATOMIC_OP_FETCH(uint32_t, or, u32)
PTK_ATOMIC_OP(uint32_t, fetch_xor, u32)
PTK_ATOMIC_OP_FETCH(uint32_t, xor, u32)
PTK_ATOMIC_CAS(uint32_t, u32)

//=== u64
PTK_ATOMIC_LOAD(uint64_t, u64)
PTK_ATOMIC_STORE(uint64_t, u64)
PTK_ATOMIC_OP(uint64_t, fetch_add, u64)
PTK_ATOMIC_OP_FETCH(uint64_t, add, u64)
PTK_ATOMIC_OP(uint64_t, fetch_sub, u64)
PTK_ATOMIC_OP_FETCH(uint64_t, sub, u64)
PTK_ATOMIC_OP(uint64_t, fetch_and, u64)
PTK_ATOMIC_OP_FETCH(uint64_t, and, u64)
PTK_ATOMIC_OP(uint64_t, fetch_or, u64)
PTK_ATOMIC_OP_FETCH(uint64_t, or, u64)
PTK_ATOMIC_OP(uint64_t, fetch_xor, u64)
PTK_ATOMIC_OP_FETCH(uint64_t, xor, u64)
PTK_ATOMIC_CAS(uint64_t, u64)

//=== pointer
ptk_err ptk_atomic_load_ptr(void **dest, ptk_atomic void **src) {
    *dest = atomic_load((_Atomic void **)src);
    return PTK_OK;
}

ptk_err ptk_atomic_store_ptr(ptk_atomic void **dest, void *src) {
    atomic_store((_Atomic void **)dest, src);
    return PTK_OK;
}

ptk_err ptk_atomic_compare_and_swap_ptr(ptk_atomic void **dest, void *old_val, void *new_val) {
    atomic_compare_exchange_strong((_Atomic void **)dest, &old_val, new_val);
    return PTK_OK;
}
