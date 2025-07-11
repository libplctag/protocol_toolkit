#include "ptk_atomic.h"

#if defined(__GNUC__) || defined(__clang__)
// GCC/Clang atomic builtins

#define PTK_ATOMIC_LOAD(type, func_suffix) \
    ptk_err ptk_atomic_load_##func_suffix(type *dest_value, ptk_atomic type *src_value) { \
        *dest_value = __atomic_load_n(src_value, __ATOMIC_SEQ_CST); \
        return PTK_OK; \
    }

#define PTK_ATOMIC_STORE(type, func_suffix) \
    ptk_err ptk_atomic_store_##func_suffix(ptk_atomic type *dest_value, type src_value) { \
        __atomic_store_n(dest_value, src_value, __ATOMIC_SEQ_CST); \
        return PTK_OK; \
    }

#define PTK_ATOMIC_FETCH_OP(type, op, func_suffix) \
    ptk_err ptk_atomic_fetch_##op##_##func_suffix(ptk_atomic type *dest_value, type src_value) { \
        __atomic_fetch_##op(dest_value, src_value, __ATOMIC_SEQ_CST); \
        return PTK_OK; \
    }

#define PTK_ATOMIC_OP_FETCH(type, op, func_suffix) \
    ptk_err ptk_atomic_##op##_fetch_##func_suffix(ptk_atomic type *dest_value, type src_value) { \
        __atomic_##op##_fetch(dest_value, src_value, __ATOMIC_SEQ_CST); \
        return PTK_OK; \
    }

#define PTK_ATOMIC_CAS(type, func_suffix) \
    ptk_err ptk_atomic_compare_and_swap_##func_suffix(ptk_atomic type *dest_value, type old_value, type new_value) { \
        __atomic_compare_exchange_n(dest_value, &old_value, new_value, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); \
        return PTK_OK; \
    }

#elif defined(_MSC_VER)
// MSVC intrinsics
#include <intrin.h>

#define PTK_ATOMIC_LOAD(type, func_suffix) \
    ptk_err ptk_atomic_load_##func_suffix(type *dest_value, ptk_atomic type *src_value) { \
        *dest_value = *src_value; \
        _ReadWriteBarrier(); \
        return PTK_OK; \
    }

#define PTK_ATOMIC_STORE(type, func_suffix) \
    ptk_err ptk_atomic_store_##func_suffix(ptk_atomic type *dest_value, type src_value) { \
        _ReadWriteBarrier(); \
        *dest_value = src_value; \
        _ReadWriteBarrier(); \
        return PTK_OK; \
    }

// For MSVC, we'll implement basic versions - could be improved with proper intrinsics
#define PTK_ATOMIC_FETCH_OP(type, op, func_suffix) \
    ptk_err ptk_atomic_fetch_##op##_##func_suffix(ptk_atomic type *dest_value, type src_value) { \
        return PTK_ERR_UNSUPPORTED; \
    }

#define PTK_ATOMIC_OP_FETCH(type, op, func_suffix) \
    ptk_err ptk_atomic_##op##_fetch_##func_suffix(ptk_atomic type *dest_value, type src_value) { \
        return PTK_ERR_UNSUPPORTED; \
    }

#define PTK_ATOMIC_CAS(type, func_suffix) \
    ptk_err ptk_atomic_compare_and_swap_##func_suffix(ptk_atomic type *dest_value, type old_value, type new_value) { \
        return PTK_ERR_UNSUPPORTED; \
    }

#else
// Fallback implementation using volatile (not truly atomic!)
#define PTK_ATOMIC_LOAD(type, func_suffix) \
    ptk_err ptk_atomic_load_##func_suffix(type *dest_value, ptk_atomic type *src_value) { \
        *dest_value = *src_value; \
        return PTK_OK; \
    }

#define PTK_ATOMIC_STORE(type, func_suffix) \
    ptk_err ptk_atomic_store_##func_suffix(ptk_atomic type *dest_value, type src_value) { \
        *dest_value = src_value; \
        return PTK_OK; \
    }

#define PTK_ATOMIC_FETCH_OP(type, op, func_suffix) \
    ptk_err ptk_atomic_fetch_##op##_##func_suffix(ptk_atomic type *dest_value, type src_value) { \
        return PTK_ERR_UNSUPPORTED; \
    }

#define PTK_ATOMIC_OP_FETCH(type, op, func_suffix) \
    ptk_err ptk_atomic_##op##_fetch_##func_suffix(ptk_atomic type *dest_value, type src_value) { \
        return PTK_ERR_UNSUPPORTED; \
    }

#define PTK_ATOMIC_CAS(type, func_suffix) \
    ptk_err ptk_atomic_compare_and_swap_##func_suffix(ptk_atomic type *dest_value, type old_value, type new_value) { \
        return PTK_ERR_UNSUPPORTED; \
    }

#endif

// Generate all u8 functions
PTK_ATOMIC_LOAD(uint8_t, u8)
PTK_ATOMIC_STORE(uint8_t, u8)
PTK_ATOMIC_FETCH_OP(uint8_t, add, u8)
PTK_ATOMIC_OP_FETCH(uint8_t, add, u8)
PTK_ATOMIC_FETCH_OP(uint8_t, sub, u8)
PTK_ATOMIC_OP_FETCH(uint8_t, sub, u8)
PTK_ATOMIC_FETCH_OP(uint8_t, and, u8)
PTK_ATOMIC_OP_FETCH(uint8_t, and, u8)
PTK_ATOMIC_FETCH_OP(uint8_t, or, u8)
PTK_ATOMIC_OP_FETCH(uint8_t, or, u8)
PTK_ATOMIC_FETCH_OP(uint8_t, xor, u8)
PTK_ATOMIC_OP_FETCH(uint8_t, xor, u8)

// Generate all u16 functions
PTK_ATOMIC_LOAD(uint16_t, u16)
PTK_ATOMIC_STORE(uint16_t, u16)
PTK_ATOMIC_FETCH_OP(uint16_t, add, u16)
PTK_ATOMIC_OP_FETCH(uint16_t, add, u16)
PTK_ATOMIC_FETCH_OP(uint16_t, sub, u16)
PTK_ATOMIC_OP_FETCH(uint16_t, sub, u16)
PTK_ATOMIC_FETCH_OP(uint16_t, and, u16)
PTK_ATOMIC_OP_FETCH(uint16_t, and, u16)
PTK_ATOMIC_FETCH_OP(uint16_t, or, u16)
PTK_ATOMIC_OP_FETCH(uint16_t, or, u16)
PTK_ATOMIC_FETCH_OP(uint16_t, xor, u16)
PTK_ATOMIC_OP_FETCH(uint16_t, xor, u16)
PTK_ATOMIC_CAS(uint16_t, u16)

// Generate all u32 functions
PTK_ATOMIC_LOAD(uint32_t, u32)
PTK_ATOMIC_STORE(uint32_t, u32)
PTK_ATOMIC_FETCH_OP(uint32_t, add, u32)
PTK_ATOMIC_OP_FETCH(uint32_t, add, u32)
PTK_ATOMIC_FETCH_OP(uint32_t, sub, u32)
PTK_ATOMIC_OP_FETCH(uint32_t, sub, u32)
PTK_ATOMIC_FETCH_OP(uint32_t, and, u32)
PTK_ATOMIC_OP_FETCH(uint32_t, and, u32)
PTK_ATOMIC_FETCH_OP(uint32_t, or, u32)
PTK_ATOMIC_OP_FETCH(uint32_t, or, u32)
PTK_ATOMIC_FETCH_OP(uint32_t, xor, u32)
PTK_ATOMIC_OP_FETCH(uint32_t, xor, u32)
PTK_ATOMIC_CAS(uint32_t, u32)

// Generate all u64 functions
PTK_ATOMIC_LOAD(uint64_t, u64)
PTK_ATOMIC_STORE(uint64_t, u64)
PTK_ATOMIC_FETCH_OP(uint64_t, add, u64)
PTK_ATOMIC_OP_FETCH(uint64_t, add, u64)
PTK_ATOMIC_FETCH_OP(uint64_t, sub, u64)
PTK_ATOMIC_OP_FETCH(uint64_t, sub, u64)
PTK_ATOMIC_FETCH_OP(uint64_t, and, u64)
PTK_ATOMIC_OP_FETCH(uint64_t, and, u64)
PTK_ATOMIC_FETCH_OP(uint64_t, or, u64)
PTK_ATOMIC_OP_FETCH(uint64_t, or, u64)
PTK_ATOMIC_FETCH_OP(uint64_t, xor, u64)
PTK_ATOMIC_OP_FETCH(uint64_t, xor, u64)
PTK_ATOMIC_CAS(uint64_t, u64)

// Special case for u8 CAS (missing from the pattern above)
PTK_ATOMIC_CAS(uint8_t, u8)

// Pointer functions
ptk_err ptk_atomic_load_ptr(void **dest_value, ptk_atomic void **src_value) {
#if defined(__GNUC__) || defined(__clang__)
    *dest_value = __atomic_load_n(src_value, __ATOMIC_SEQ_CST);
#else
    *dest_value = *src_value;
#endif
    return PTK_OK;
}

ptk_err ptk_atomic_store_ptr(ptk_atomic void **dest_value, void *src_value) {
#if defined(__GNUC__) || defined(__clang__)
    __atomic_store_n(dest_value, src_value, __ATOMIC_SEQ_CST);
#else
    *dest_value = src_value;
#endif
    return PTK_OK;
}

ptk_err ptk_atomic_compare_and_swap_ptr(ptk_atomic void **dest_value, void *old_value, void *new_value) {
#if defined(__GNUC__) || defined(__clang__)
    __atomic_compare_exchange_n(dest_value, &old_value, new_value, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return PTK_OK;
#else
    return PTK_ERR_UNSUPPORTED;
#endif
}