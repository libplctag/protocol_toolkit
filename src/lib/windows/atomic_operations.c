#include "../ptk_atomic.h"

#ifdef _WIN32
#include <intrin.h>
#include <Windows.h>

// Windows-specific atomic implementation using Windows API and intrinsics

// 8-bit operations
ptk_err ptk_atomic_load_u8(uint8_t *dest_value, ptk_atomic uint8_t *src_value) {
    *dest_value = *(volatile uint8_t *)src_value;
    _ReadWriteBarrier();
    return PTK_OK;
}

ptk_err ptk_atomic_store_u8(ptk_atomic uint8_t *dest_value, uint8_t src_value) {
    _ReadWriteBarrier();
    *(volatile uint8_t *)dest_value = src_value;
    _ReadWriteBarrier();
    return PTK_OK;
}

ptk_err ptk_atomic_fetch_add_u8(ptk_atomic uint8_t *dest_value, uint8_t src_value) {
    _InterlockedExchangeAdd8((char *)dest_value, src_value);
    return PTK_OK;
}

ptk_err ptk_atomic_add_fetch_u8(ptk_atomic uint8_t *dest_value, uint8_t src_value) {
    _InterlockedExchangeAdd8((char *)dest_value, src_value);
    return PTK_OK;
}

ptk_err ptk_atomic_fetch_sub_u8(ptk_atomic uint8_t *dest_value, uint8_t src_value) {
    _InterlockedExchangeAdd8((char *)dest_value, -(char)src_value);
    return PTK_OK;
}

ptk_err ptk_atomic_sub_fetch_u8(ptk_atomic uint8_t *dest_value, uint8_t src_value) {
    _InterlockedExchangeAdd8((char *)dest_value, -(char)src_value);
    return PTK_OK;
}

ptk_err ptk_atomic_fetch_and_u8(ptk_atomic uint8_t *dest_value, uint8_t src_value) {
    _InterlockedAnd8((char *)dest_value, src_value);
    return PTK_OK;
}

ptk_err ptk_atomic_and_fetch_u8(ptk_atomic uint8_t *dest_value, uint8_t src_value) {
    _InterlockedAnd8((char *)dest_value, src_value);
    return PTK_OK;
}

ptk_err ptk_atomic_fetch_or_u8(ptk_atomic uint8_t *dest_value, uint8_t src_value) {
    _InterlockedOr8((char *)dest_value, src_value);
    return PTK_OK;
}

ptk_err ptk_atomic_or_fetch_u8(ptk_atomic uint8_t *dest_value, uint8_t src_value) {
    _InterlockedOr8((char *)dest_value, src_value);
    return PTK_OK;
}

ptk_err ptk_atomic_fetch_xor_u8(ptk_atomic uint8_t *dest_value, uint8_t src_value) {
    _InterlockedXor8((char *)dest_value, src_value);
    return PTK_OK;
}

ptk_err ptk_atomic_xor_fetch_u8(ptk_atomic uint8_t *dest_value, uint8_t src_value) {
    _InterlockedXor8((char *)dest_value, src_value);
    return PTK_OK;
}

ptk_err ptk_atomic_compare_and_swap_u8(ptk_atomic uint8_t *dest_value, uint8_t old_value, uint8_t new_value) {
    _InterlockedCompareExchange8((char *)dest_value, new_value, old_value);
    return PTK_OK;
}

// 16-bit operations
ptk_err ptk_atomic_load_u16(uint16_t *dest_value, ptk_atomic uint16_t *src_value) {
    *dest_value = *(volatile uint16_t *)src_value;
    _ReadWriteBarrier();
    return PTK_OK;
}

ptk_err ptk_atomic_store_u16(ptk_atomic uint16_t *dest_value, uint16_t src_value) {
    _ReadWriteBarrier();
    *(volatile uint16_t *)dest_value = src_value;
    _ReadWriteBarrier();
    return PTK_OK;
}

ptk_err ptk_atomic_fetch_add_u16(ptk_atomic uint16_t *dest_value, uint16_t src_value) {
    _InterlockedExchangeAdd16((short *)dest_value, src_value);
    return PTK_OK;
}

ptk_err ptk_atomic_add_fetch_u16(ptk_atomic uint16_t *dest_value, uint16_t src_value) {
    _InterlockedExchangeAdd16((short *)dest_value, src_value);
    return PTK_OK;
}

ptk_err ptk_atomic_fetch_sub_u16(ptk_atomic uint16_t *dest_value, uint16_t src_value) {
    _InterlockedExchangeAdd16((short *)dest_value, -(short)src_value);
    return PTK_OK;
}

ptk_err ptk_atomic_sub_fetch_u16(ptk_atomic uint16_t *dest_value, uint16_t src_value) {
    _InterlockedExchangeAdd16((short *)dest_value, -(short)src_value);
    return PTK_OK;
}

ptk_err ptk_atomic_fetch_and_u16(ptk_atomic uint16_t *dest_value, uint16_t src_value) {
    _InterlockedAnd16((short *)dest_value, src_value);
    return PTK_OK;
}

ptk_err ptk_atomic_and_fetch_u16(ptk_atomic uint16_t *dest_value, uint16_t src_value) {
    _InterlockedAnd16((short *)dest_value, src_value);
    return PTK_OK;
}

ptk_err ptk_atomic_fetch_or_u16(ptk_atomic uint16_t *dest_value, uint16_t src_value) {
    _InterlockedOr16((short *)dest_value, src_value);
    return PTK_OK;
}

ptk_err ptk_atomic_or_fetch_u16(ptk_atomic uint16_t *dest_value, uint16_t src_value) {
    _InterlockedOr16((short *)dest_value, src_value);
    return PTK_OK;
}

ptk_err ptk_atomic_fetch_xor_u16(ptk_atomic uint16_t *dest_value, uint16_t src_value) {
    _InterlockedXor16((short *)dest_value, src_value);
    return PTK_OK;
}

ptk_err ptk_atomic_xor_fetch_u16(ptk_atomic uint16_t *dest_value, uint16_t src_value) {
    _InterlockedXor16((short *)dest_value, src_value);
    return PTK_OK;
}

ptk_err ptk_atomic_compare_and_swap_u16(ptk_atomic uint16_t *dest_value, uint16_t old_value, uint16_t new_value) {
    _InterlockedCompareExchange16((short *)dest_value, new_value, old_value);
    return PTK_OK;
}

// 32-bit operations
ptk_err ptk_atomic_load_u32(uint32_t *dest_value, ptk_atomic uint32_t *src_value) {
    *dest_value = *(volatile uint32_t *)src_value;
    _ReadWriteBarrier();
    return PTK_OK;
}

ptk_err ptk_atomic_store_u32(ptk_atomic uint32_t *dest_value, uint32_t src_value) {
    _ReadWriteBarrier();
    *(volatile uint32_t *)dest_value = src_value;
    _ReadWriteBarrier();
    return PTK_OK;
}

ptk_err ptk_atomic_fetch_add_u32(ptk_atomic uint32_t *dest_value, uint32_t src_value) {
    _InterlockedExchangeAdd((long *)dest_value, src_value);
    return PTK_OK;
}

ptk_err ptk_atomic_add_fetch_u32(ptk_atomic uint32_t *dest_value, uint32_t src_value) {
    _InterlockedExchangeAdd((long *)dest_value, src_value);
    return PTK_OK;
}

ptk_err ptk_atomic_fetch_sub_u32(ptk_atomic uint32_t *dest_value, uint32_t src_value) {
    _InterlockedExchangeAdd((long *)dest_value, -(long)src_value);
    return PTK_OK;
}

ptk_err ptk_atomic_sub_fetch_u32(ptk_atomic uint32_t *dest_value, uint32_t src_value) {
    _InterlockedExchangeAdd((long *)dest_value, -(long)src_value);
    return PTK_OK;
}

ptk_err ptk_atomic_fetch_and_u32(ptk_atomic uint32_t *dest_value, uint32_t src_value) {
    _InterlockedAnd((long *)dest_value, src_value);
    return PTK_OK;
}

ptk_err ptk_atomic_and_fetch_u32(ptk_atomic uint32_t *dest_value, uint32_t src_value) {
    _InterlockedAnd((long *)dest_value, src_value);
    return PTK_OK;
}

ptk_err ptk_atomic_fetch_or_u32(ptk_atomic uint32_t *dest_value, uint32_t src_value) {
    _InterlockedOr((long *)dest_value, src_value);
    return PTK_OK;
}

ptk_err ptk_atomic_or_fetch_u32(ptk_atomic uint32_t *dest_value, uint32_t src_value) {
    _InterlockedOr((long *)dest_value, src_value);
    return PTK_OK;
}

ptk_err ptk_atomic_fetch_xor_u32(ptk_atomic uint32_t *dest_value, uint32_t src_value) {
    _InterlockedXor((long *)dest_value, src_value);
    return PTK_OK;
}

ptk_err ptk_atomic_xor_fetch_u32(ptk_atomic uint32_t *dest_value, uint32_t src_value) {
    _InterlockedXor((long *)dest_value, src_value);
    return PTK_OK;
}

ptk_err ptk_atomic_compare_and_swap_u32(ptk_atomic uint32_t *dest_value, uint32_t old_value, uint32_t new_value) {
    _InterlockedCompareExchange((long *)dest_value, new_value, old_value);
    return PTK_OK;
}

// 64-bit operations
ptk_err ptk_atomic_load_u64(uint64_t *dest_value, ptk_atomic uint64_t *src_value) {
    *dest_value = *(volatile uint64_t *)src_value;
    _ReadWriteBarrier();
    return PTK_OK;
}

ptk_err ptk_atomic_store_u64(ptk_atomic uint64_t *dest_value, uint64_t src_value) {
    _ReadWriteBarrier();
    *(volatile uint64_t *)dest_value = src_value;
    _ReadWriteBarrier();
    return PTK_OK;
}

ptk_err ptk_atomic_fetch_add_u64(ptk_atomic uint64_t *dest_value, uint64_t src_value) {
    _InterlockedExchangeAdd64((__int64 *)dest_value, src_value);
    return PTK_OK;
}

ptk_err ptk_atomic_add_fetch_u64(ptk_atomic uint64_t *dest_value, uint64_t src_value) {
    _InterlockedExchangeAdd64((__int64 *)dest_value, src_value);
    return PTK_OK;
}

ptk_err ptk_atomic_fetch_sub_u64(ptk_atomic uint64_t *dest_value, uint64_t src_value) {
    _InterlockedExchangeAdd64((__int64 *)dest_value, -(__int64)src_value);
    return PTK_OK;
}

ptk_err ptk_atomic_sub_fetch_u64(ptk_atomic uint64_t *dest_value, uint64_t src_value) {
    _InterlockedExchangeAdd64((__int64 *)dest_value, -(__int64)src_value);
    return PTK_OK;
}

ptk_err ptk_atomic_fetch_and_u64(ptk_atomic uint64_t *dest_value, uint64_t src_value) {
    _InterlockedAnd64((__int64 *)dest_value, src_value);
    return PTK_OK;
}

ptk_err ptk_atomic_and_fetch_u64(ptk_atomic uint64_t *dest_value, uint64_t src_value) {
    _InterlockedAnd64((__int64 *)dest_value, src_value);
    return PTK_OK;
}

ptk_err ptk_atomic_fetch_or_u64(ptk_atomic uint64_t *dest_value, uint64_t src_value) {
    _InterlockedOr64((__int64 *)dest_value, src_value);
    return PTK_OK;
}

ptk_err ptk_atomic_or_fetch_u64(ptk_atomic uint64_t *dest_value, uint64_t src_value) {
    _InterlockedOr64((__int64 *)dest_value, src_value);
    return PTK_OK;
}

ptk_err ptk_atomic_fetch_xor_u64(ptk_atomic uint64_t *dest_value, uint64_t src_value) {
    _InterlockedXor64((__int64 *)dest_value, src_value);
    return PTK_OK;
}

ptk_err ptk_atomic_xor_fetch_u64(ptk_atomic uint64_t *dest_value, uint64_t src_value) {
    _InterlockedXor64((__int64 *)dest_value, src_value);
    return PTK_OK;
}

ptk_err ptk_atomic_compare_and_swap_u64(ptk_atomic uint64_t *dest_value, uint64_t old_value, uint64_t new_value) {
    _InterlockedCompareExchange64((__int64 *)dest_value, new_value, old_value);
    return PTK_OK;
}

// Pointer operations
ptk_err ptk_atomic_load_ptr(void **dest_value, ptk_atomic void **src_value) {
    *dest_value = *(void * volatile *)src_value;
    _ReadWriteBarrier();
    return PTK_OK;
}

ptk_err ptk_atomic_store_ptr(ptk_atomic void **dest_value, void *src_value) {
    _ReadWriteBarrier();
    *(void * volatile *)dest_value = src_value;
    _ReadWriteBarrier();
    return PTK_OK;
}

ptk_err ptk_atomic_compare_and_swap_ptr(ptk_atomic void **dest_value, void *old_value, void *new_value) {
#ifdef _WIN64
    _InterlockedCompareExchangePointer(dest_value, new_value, old_value);
#else
    _InterlockedCompareExchange((long *)dest_value, (long)new_value, (long)old_value);
#endif
    return PTK_OK;
}

#endif