#include "../ptk_atomic.h"

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#include <stdatomic.h>

// C11 atomic implementation for POSIX systems

#define POSIX_ATOMIC_LOAD(type, func_suffix) \
    ptk_err ptk_atomic_load_##func_suffix(type *dest_value, ptk_atomic type *src_value) { \
        *dest_value = atomic_load((_Atomic type *)src_value); \
        return PTK_OK; \
    }

#define POSIX_ATOMIC_STORE(type, func_suffix) \
    ptk_err ptk_atomic_store_##func_suffix(ptk_atomic type *dest_value, type src_value) { \
        atomic_store((_Atomic type *)dest_value, src_value); \
        return PTK_OK; \
    }

#define POSIX_ATOMIC_FETCH_ADD(type, func_suffix) \
    ptk_err ptk_atomic_fetch_add_##func_suffix(ptk_atomic type *dest_value, type src_value) { \
        atomic_fetch_add((_Atomic type *)dest_value, src_value); \
        return PTK_OK; \
    }

#define POSIX_ATOMIC_ADD_FETCH(type, func_suffix) \
    ptk_err ptk_atomic_add_fetch_##func_suffix(ptk_atomic type *dest_value, type src_value) { \
        atomic_fetch_add((_Atomic type *)dest_value, src_value); \
        return PTK_OK; \
    }

#define POSIX_ATOMIC_FETCH_SUB(type, func_suffix) \
    ptk_err ptk_atomic_fetch_sub_##func_suffix(ptk_atomic type *dest_value, type src_value) { \
        atomic_fetch_sub((_Atomic type *)dest_value, src_value); \
        return PTK_OK; \
    }

#define POSIX_ATOMIC_SUB_FETCH(type, func_suffix) \
    ptk_err ptk_atomic_sub_fetch_##func_suffix(ptk_atomic type *dest_value, type src_value) { \
        atomic_fetch_sub((_Atomic type *)dest_value, src_value); \
        return PTK_OK; \
    }

#define POSIX_ATOMIC_FETCH_AND(type, func_suffix) \
    ptk_err ptk_atomic_fetch_and_##func_suffix(ptk_atomic type *dest_value, type src_value) { \
        atomic_fetch_and((_Atomic type *)dest_value, src_value); \
        return PTK_OK; \
    }

#define POSIX_ATOMIC_AND_FETCH(type, func_suffix) \
    ptk_err ptk_atomic_and_fetch_##func_suffix(ptk_atomic type *dest_value, type src_value) { \
        atomic_fetch_and((_Atomic type *)dest_value, src_value); \
        return PTK_OK; \
    }

#define POSIX_ATOMIC_FETCH_OR(type, func_suffix) \
    ptk_err ptk_atomic_fetch_or_##func_suffix(ptk_atomic type *dest_value, type src_value) { \
        atomic_fetch_or((_Atomic type *)dest_value, src_value); \
        return PTK_OK; \
    }

#define POSIX_ATOMIC_OR_FETCH(type, func_suffix) \
    ptk_err ptk_atomic_or_fetch_##func_suffix(ptk_atomic type *dest_value, type src_value) { \
        atomic_fetch_or((_Atomic type *)dest_value, src_value); \
        return PTK_OK; \
    }

#define POSIX_ATOMIC_FETCH_XOR(type, func_suffix) \
    ptk_err ptk_atomic_fetch_xor_##func_suffix(ptk_atomic type *dest_value, type src_value) { \
        atomic_fetch_xor((_Atomic type *)dest_value, src_value); \
        return PTK_OK; \
    }

#define POSIX_ATOMIC_XOR_FETCH(type, func_suffix) \
    ptk_err ptk_atomic_xor_fetch_##func_suffix(ptk_atomic type *dest_value, type src_value) { \
        atomic_fetch_xor((_Atomic type *)dest_value, src_value); \
        return PTK_OK; \
    }

#define POSIX_ATOMIC_CAS(type, func_suffix) \
    ptk_err ptk_atomic_compare_and_swap_##func_suffix(ptk_atomic type *dest_value, type old_value, type new_value) { \
        type expected = old_value; \
        atomic_compare_exchange_strong((_Atomic type *)dest_value, &expected, new_value); \
        return PTK_OK; \
    }

#define POSIX_ATOMIC_ALL_OPS(type, func_suffix) \
    POSIX_ATOMIC_LOAD(type, func_suffix) \
    POSIX_ATOMIC_STORE(type, func_suffix) \
    POSIX_ATOMIC_FETCH_ADD(type, func_suffix) \
    POSIX_ATOMIC_ADD_FETCH(type, func_suffix) \
    POSIX_ATOMIC_FETCH_SUB(type, func_suffix) \
    POSIX_ATOMIC_SUB_FETCH(type, func_suffix) \
    POSIX_ATOMIC_FETCH_AND(type, func_suffix) \
    POSIX_ATOMIC_AND_FETCH(type, func_suffix) \
    POSIX_ATOMIC_FETCH_OR(type, func_suffix) \
    POSIX_ATOMIC_OR_FETCH(type, func_suffix) \
    POSIX_ATOMIC_FETCH_XOR(type, func_suffix) \
    POSIX_ATOMIC_XOR_FETCH(type, func_suffix) \
    POSIX_ATOMIC_CAS(type, func_suffix)

// Generate implementations for all types
POSIX_ATOMIC_ALL_OPS(uint8_t, u8)
POSIX_ATOMIC_ALL_OPS(uint16_t, u16)
POSIX_ATOMIC_ALL_OPS(uint32_t, u32)
POSIX_ATOMIC_ALL_OPS(uint64_t, u64)

// Pointer operations
ptk_err ptk_atomic_load_ptr(void **dest_value, ptk_atomic void **src_value) {
    *dest_value = atomic_load((_Atomic void **)src_value);
    return PTK_OK;
}

ptk_err ptk_atomic_store_ptr(ptk_atomic void **dest_value, void *src_value) {
    atomic_store((_Atomic void **)dest_value, src_value);
    return PTK_OK;
}

ptk_err ptk_atomic_compare_and_swap_ptr(ptk_atomic void **dest_value, void *old_value, void *new_value) {
    void *expected = old_value;
    atomic_compare_exchange_strong((_Atomic void **)dest_value, &expected, new_value);
    return PTK_OK;
}

#else
// Fallback to GCC/Clang builtins for older C standards

#define POSIX_ATOMIC_LOAD(type, func_suffix) \
    ptk_err ptk_atomic_load_##func_suffix(type *dest_value, ptk_atomic type *src_value) { \
        *dest_value = __atomic_load_n(src_value, __ATOMIC_SEQ_CST); \
        return PTK_OK; \
    }

#define POSIX_ATOMIC_STORE(type, func_suffix) \
    ptk_err ptk_atomic_store_##func_suffix(ptk_atomic type *dest_value, type src_value) { \
        __atomic_store_n(dest_value, src_value, __ATOMIC_SEQ_CST); \
        return PTK_OK; \
    }

#define POSIX_ATOMIC_FETCH_ADD(type, func_suffix) \
    ptk_err ptk_atomic_fetch_add_##func_suffix(ptk_atomic type *dest_value, type src_value) { \
        __atomic_fetch_add(dest_value, src_value, __ATOMIC_SEQ_CST); \
        return PTK_OK; \
    }

#define POSIX_ATOMIC_ADD_FETCH(type, func_suffix) \
    ptk_err ptk_atomic_add_fetch_##func_suffix(ptk_atomic type *dest_value, type src_value) { \
        __atomic_add_fetch(dest_value, src_value, __ATOMIC_SEQ_CST); \
        return PTK_OK; \
    }

#define POSIX_ATOMIC_FETCH_SUB(type, func_suffix) \
    ptk_err ptk_atomic_fetch_sub_##func_suffix(ptk_atomic type *dest_value, type src_value) { \
        __atomic_fetch_sub(dest_value, src_value, __ATOMIC_SEQ_CST); \
        return PTK_OK; \
    }

#define POSIX_ATOMIC_SUB_FETCH(type, func_suffix) \
    ptk_err ptk_atomic_sub_fetch_##func_suffix(ptk_atomic type *dest_value, type src_value) { \
        __atomic_sub_fetch(dest_value, src_value, __ATOMIC_SEQ_CST); \
        return PTK_OK; \
    }

#define POSIX_ATOMIC_FETCH_AND(type, func_suffix) \
    ptk_err ptk_atomic_fetch_and_##func_suffix(ptk_atomic type *dest_value, type src_value) { \
        __atomic_fetch_and(dest_value, src_value, __ATOMIC_SEQ_CST); \
        return PTK_OK; \
    }

#define POSIX_ATOMIC_AND_FETCH(type, func_suffix) \
    ptk_err ptk_atomic_and_fetch_##func_suffix(ptk_atomic type *dest_value, type src_value) { \
        __atomic_and_fetch(dest_value, src_value, __ATOMIC_SEQ_CST); \
        return PTK_OK; \
    }

#define POSIX_ATOMIC_FETCH_OR(type, func_suffix) \
    ptk_err ptk_atomic_fetch_or_##func_suffix(ptk_atomic type *dest_value, type src_value) { \
        __atomic_fetch_or(dest_value, src_value, __ATOMIC_SEQ_CST); \
        return PTK_OK; \
    }

#define POSIX_ATOMIC_OR_FETCH(type, func_suffix) \
    ptk_err ptk_atomic_or_fetch_##func_suffix(ptk_atomic type *dest_value, type src_value) { \
        __atomic_or_fetch(dest_value, src_value, __ATOMIC_SEQ_CST); \
        return PTK_OK; \
    }

#define POSIX_ATOMIC_FETCH_XOR(type, func_suffix) \
    ptk_err ptk_atomic_fetch_xor_##func_suffix(ptk_atomic type *dest_value, type src_value) { \
        __atomic_fetch_xor(dest_value, src_value, __ATOMIC_SEQ_CST); \
        return PTK_OK; \
    }

#define POSIX_ATOMIC_XOR_FETCH(type, func_suffix) \
    ptk_err ptk_atomic_xor_fetch_##func_suffix(ptk_atomic type *dest_value, type src_value) { \
        __atomic_xor_fetch(dest_value, src_value, __ATOMIC_SEQ_CST); \
        return PTK_OK; \
    }

#define POSIX_ATOMIC_CAS(type, func_suffix) \
    ptk_err ptk_atomic_compare_and_swap_##func_suffix(ptk_atomic type *dest_value, type old_value, type new_value) { \
        __atomic_compare_exchange_n(dest_value, &old_value, new_value, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); \
        return PTK_OK; \
    }

#define POSIX_ATOMIC_ALL_OPS(type, func_suffix) \
    POSIX_ATOMIC_LOAD(type, func_suffix) \
    POSIX_ATOMIC_STORE(type, func_suffix) \
    POSIX_ATOMIC_FETCH_ADD(type, func_suffix) \
    POSIX_ATOMIC_ADD_FETCH(type, func_suffix) \
    POSIX_ATOMIC_FETCH_SUB(type, func_suffix) \
    POSIX_ATOMIC_SUB_FETCH(type, func_suffix) \
    POSIX_ATOMIC_FETCH_AND(type, func_suffix) \
    POSIX_ATOMIC_AND_FETCH(type, func_suffix) \
    POSIX_ATOMIC_FETCH_OR(type, func_suffix) \
    POSIX_ATOMIC_OR_FETCH(type, func_suffix) \
    POSIX_ATOMIC_FETCH_XOR(type, func_suffix) \
    POSIX_ATOMIC_XOR_FETCH(type, func_suffix) \
    POSIX_ATOMIC_CAS(type, func_suffix)

// Generate implementations for all types
POSIX_ATOMIC_ALL_OPS(uint8_t, u8)
POSIX_ATOMIC_ALL_OPS(uint16_t, u16)
POSIX_ATOMIC_ALL_OPS(uint32_t, u32)
POSIX_ATOMIC_ALL_OPS(uint64_t, u64)

// Pointer operations
ptk_err ptk_atomic_load_ptr(void **dest_value, ptk_atomic void **src_value) {
    *dest_value = __atomic_load_n(src_value, __ATOMIC_SEQ_CST);
    return PTK_OK;
}

ptk_err ptk_atomic_store_ptr(ptk_atomic void **dest_value, void *src_value) {
    __atomic_store_n(dest_value, src_value, __ATOMIC_SEQ_CST);
    return PTK_OK;
}

ptk_err ptk_atomic_compare_and_swap_ptr(ptk_atomic void **dest_value, void *old_value, void *new_value) {
    __atomic_compare_exchange_n(dest_value, &old_value, new_value, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return PTK_OK;
}

#endif