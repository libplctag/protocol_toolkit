#include <ptk_atomic.h>

#ifdef _WIN32
#include <intrin.h>
#include <Windows.h>

// Windows-specific atomic implementation using Windows API and intrinsics

// 8-bit operations
uint8_t ptk_atomic_load_u8(ptk_atomic uint8_t *src_value) {
    uint8_t result = *(volatile uint8_t *)src_value;
    _ReadWriteBarrier();
    return result;
}

ptk_err_t ptk_atomic_store_u8(ptk_atomic uint8_t *dest_value, uint8_t src_value) {
    _ReadWriteBarrier();
    *(volatile uint8_t *)dest_value = src_value;
    _ReadWriteBarrier();
    return PTK_OK;
}

uint8_t ptk_atomic_fetch_add_u8(ptk_atomic uint8_t *dest_value, uint8_t src_value) {
    return _InterlockedExchangeAdd8((char *)dest_value, src_value);
}

uint8_t ptk_atomic_add_fetch_u8(ptk_atomic uint8_t *dest_value, uint8_t src_value) {
    return _InterlockedExchangeAdd8((char *)dest_value, src_value) + src_value;
}

uint8_t ptk_atomic_fetch_sub_u8(ptk_atomic uint8_t *dest_value, uint8_t src_value) {
    return _InterlockedExchangeAdd8((char *)dest_value, -(char)src_value);
}

uint8_t ptk_atomic_sub_fetch_u8(ptk_atomic uint8_t *dest_value, uint8_t src_value) {
    return _InterlockedExchangeAdd8((char *)dest_value, -(char)src_value) - src_value;
}

uint8_t ptk_atomic_fetch_and_u8(ptk_atomic uint8_t *dest_value, uint8_t src_value) {
    return _InterlockedAnd8((char *)dest_value, src_value);
}

uint8_t ptk_atomic_and_fetch_u8(ptk_atomic uint8_t *dest_value, uint8_t src_value) {
    return _InterlockedAnd8((char *)dest_value, src_value) & src_value;
}

uint8_t ptk_atomic_fetch_or_u8(ptk_atomic uint8_t *dest_value, uint8_t src_value) {
    return _InterlockedOr8((char *)dest_value, src_value);
}

uint8_t ptk_atomic_or_fetch_u8(ptk_atomic uint8_t *dest_value, uint8_t src_value) {
    return _InterlockedOr8((char *)dest_value, src_value) | src_value;
}

uint8_t ptk_atomic_fetch_xor_u8(ptk_atomic uint8_t *dest_value, uint8_t src_value) {
    return _InterlockedXor8((char *)dest_value, src_value);
}

uint8_t ptk_atomic_xor_fetch_u8(ptk_atomic uint8_t *dest_value, uint8_t src_value) {
    return _InterlockedXor8((char *)dest_value, src_value) ^ src_value;
}

uint8_t ptk_atomic_compare_and_swap_u8(ptk_atomic uint8_t *dest_value, uint8_t expected_old_value, uint8_t new_value) {
    return _InterlockedCompareExchange8((char *)dest_value, new_value, expected_old_value);
}

// 16-bit operations
uint16_t ptk_atomic_load_u16(ptk_atomic uint16_t *src_value) {
    uint16_t result = *(volatile uint16_t *)src_value;
    _ReadWriteBarrier();
    return result;
}

ptk_err_t ptk_atomic_store_u16(ptk_atomic uint16_t *dest_value, uint16_t src_value) {
    _ReadWriteBarrier();
    *(volatile uint16_t *)dest_value = src_value;
    _ReadWriteBarrier();
    return PTK_OK;
}

uint16_t ptk_atomic_fetch_add_u16(ptk_atomic uint16_t *dest_value, uint16_t src_value) {
    return _InterlockedExchangeAdd16((short *)dest_value, src_value);
}

uint16_t ptk_atomic_add_fetch_u16(ptk_atomic uint16_t *dest_value, uint16_t src_value) {
    return _InterlockedExchangeAdd16((short *)dest_value, src_value) + src_value;
}

uint16_t ptk_atomic_fetch_sub_u16(ptk_atomic uint16_t *dest_value, uint16_t src_value) {
    return _InterlockedExchangeAdd16((short *)dest_value, -(short)src_value);
}

uint16_t ptk_atomic_sub_fetch_u16(ptk_atomic uint16_t *dest_value, uint16_t src_value) {
    return _InterlockedExchangeAdd16((short *)dest_value, -(short)src_value) - src_value;
}

uint16_t ptk_atomic_fetch_and_u16(ptk_atomic uint16_t *dest_value, uint16_t src_value) {
    return _InterlockedAnd16((short *)dest_value, src_value);
}

uint16_t ptk_atomic_and_fetch_u16(ptk_atomic uint16_t *dest_value, uint16_t src_value) {
    return _InterlockedAnd16((short *)dest_value, src_value) & src_value;
}

uint16_t ptk_atomic_fetch_or_u16(ptk_atomic uint16_t *dest_value, uint16_t src_value) {
    return _InterlockedOr16((short *)dest_value, src_value);
}

uint16_t ptk_atomic_or_fetch_u16(ptk_atomic uint16_t *dest_value, uint16_t src_value) {
    return _InterlockedOr16((short *)dest_value, src_value) | src_value;
}

uint16_t ptk_atomic_fetch_xor_u16(ptk_atomic uint16_t *dest_value, uint16_t src_value) {
    return _InterlockedXor16((short *)dest_value, src_value);
}

uint16_t ptk_atomic_xor_fetch_u16(ptk_atomic uint16_t *dest_value, uint16_t src_value) {
    return _InterlockedXor16((short *)dest_value, src_value) ^ src_value;
}

uint16_t ptk_atomic_compare_and_swap_u16(ptk_atomic uint16_t *dest_value, uint16_t expected_old_value, uint16_t new_value) {
    return _InterlockedCompareExchange16((short *)dest_value, new_value, expected_old_value);
}

// 32-bit operations
uint32_t ptk_atomic_load_u32(ptk_atomic uint32_t *src_value) {
    uint32_t result = *(volatile uint32_t *)src_value;
    _ReadWriteBarrier();
    return result;
}

ptk_err_t ptk_atomic_store_u32(ptk_atomic uint32_t *dest_value, uint32_t src_value) {
    _ReadWriteBarrier();
    *(volatile uint32_t *)dest_value = src_value;
    _ReadWriteBarrier();
    return PTK_OK;
}

uint32_t ptk_atomic_fetch_add_u32(ptk_atomic uint32_t *dest_value, uint32_t src_value) {
    return _InterlockedExchangeAdd((long *)dest_value, src_value);
}

uint32_t ptk_atomic_add_fetch_u32(ptk_atomic uint32_t *dest_value, uint32_t src_value) {
    return _InterlockedExchangeAdd((long *)dest_value, src_value) + src_value;
}

uint32_t ptk_atomic_fetch_sub_u32(ptk_atomic uint32_t *dest_value, uint32_t src_value) {
    return _InterlockedExchangeAdd((long *)dest_value, -(long)src_value);
}

uint32_t ptk_atomic_sub_fetch_u32(ptk_atomic uint32_t *dest_value, uint32_t src_value) {
    return _InterlockedExchangeAdd((long *)dest_value, -(long)src_value) - src_value;
}

uint32_t ptk_atomic_fetch_and_u32(ptk_atomic uint32_t *dest_value, uint32_t src_value) {
    return _InterlockedAnd((long *)dest_value, src_value);
}

uint32_t ptk_atomic_and_fetch_u32(ptk_atomic uint32_t *dest_value, uint32_t src_value) {
    return _InterlockedAnd((long *)dest_value, src_value) & src_value;
}

uint32_t ptk_atomic_fetch_or_u32(ptk_atomic uint32_t *dest_value, uint32_t src_value) {
    return _InterlockedOr((long *)dest_value, src_value);
}

uint32_t ptk_atomic_or_fetch_u32(ptk_atomic uint32_t *dest_value, uint32_t src_value) {
    return _InterlockedOr((long *)dest_value, src_value) | src_value;
}

uint32_t ptk_atomic_fetch_xor_u32(ptk_atomic uint32_t *dest_value, uint32_t src_value) {
    return _InterlockedXor((long *)dest_value, src_value);
}

uint32_t ptk_atomic_xor_fetch_u32(ptk_atomic uint32_t *dest_value, uint32_t src_value) {
    return _InterlockedXor((long *)dest_value, src_value) ^ src_value;
}

uint32_t ptk_atomic_compare_and_swap_u32(ptk_atomic uint32_t *dest_value, uint32_t expected_old_value, uint32_t new_value) {
    return _InterlockedCompareExchange((long *)dest_value, new_value, expected_old_value);
}

// 64-bit operations
uint64_t ptk_atomic_load_u64(ptk_atomic uint64_t *src_value) {
    uint64_t result = *(volatile uint64_t *)src_value;
    _ReadWriteBarrier();
    return result;
}

ptk_err_t ptk_atomic_store_u64(ptk_atomic uint64_t *dest_value, uint64_t src_value) {
    _ReadWriteBarrier();
    *(volatile uint64_t *)dest_value = src_value;
    _ReadWriteBarrier();
    return PTK_OK;
}

uint64_t ptk_atomic_fetch_add_u64(ptk_atomic uint64_t *dest_value, uint64_t src_value) {
    return _InterlockedExchangeAdd64((__int64 *)dest_value, src_value);
}

uint64_t ptk_atomic_add_fetch_u64(ptk_atomic uint64_t *dest_value, uint64_t src_value) {
    return _InterlockedExchangeAdd64((__int64 *)dest_value, src_value) + src_value;
}

uint64_t ptk_atomic_fetch_sub_u64(ptk_atomic uint64_t *dest_value, uint64_t src_value) {
    return _InterlockedExchangeAdd64((__int64 *)dest_value, -(__int64)src_value);
}

uint64_t ptk_atomic_sub_fetch_u64(ptk_atomic uint64_t *dest_value, uint64_t src_value) {
    return _InterlockedExchangeAdd64((__int64 *)dest_value, -(__int64)src_value) - src_value;
}

uint64_t ptk_atomic_fetch_and_u64(ptk_atomic uint64_t *dest_value, uint64_t src_value) {
    return _InterlockedAnd64((__int64 *)dest_value, src_value);
}

uint64_t ptk_atomic_and_fetch_u64(ptk_atomic uint64_t *dest_value, uint64_t src_value) {
    return _InterlockedAnd64((__int64 *)dest_value, src_value) & src_value;
}

uint64_t ptk_atomic_fetch_or_u64(ptk_atomic uint64_t *dest_value, uint64_t src_value) {
    return _InterlockedOr64((__int64 *)dest_value, src_value);
}

uint64_t ptk_atomic_or_fetch_u64(ptk_atomic uint64_t *dest_value, uint64_t src_value) {
    return _InterlockedOr64((__int64 *)dest_value, src_value) | src_value;
}

uint64_t ptk_atomic_fetch_xor_u64(ptk_atomic uint64_t *dest_value, uint64_t src_value) {
    return _InterlockedXor64((__int64 *)dest_value, src_value);
}

uint64_t ptk_atomic_xor_fetch_u64(ptk_atomic uint64_t *dest_value, uint64_t src_value) {
    return _InterlockedXor64((__int64 *)dest_value, src_value) ^ src_value;
}

uint64_t ptk_atomic_compare_and_swap_u64(ptk_atomic uint64_t *dest_value, uint64_t expected_old_value, uint64_t new_value) {
    return _InterlockedCompareExchange64((__int64 *)dest_value, new_value, expected_old_value);
}

// Pointer operations
void *ptk_atomic_load_ptr(ptk_atomic void **src_value) {
    void *result = *(void * volatile *)src_value;
    _ReadWriteBarrier();
    return result;
}

ptk_err_t ptk_atomic_store_ptr(ptk_atomic void **dest_value, void *src_value) {
    _ReadWriteBarrier();
    *(void * volatile *)dest_value = src_value;
    _ReadWriteBarrier();
    return PTK_OK;
}

void *ptk_atomic_compare_and_swap_ptr(ptk_atomic void **dest_value, void *expected_old_value, void *new_value) {
#ifdef _WIN64
    return _InterlockedCompareExchangePointer(dest_value, new_value, expected_old_value);
#else
    return (void *)_InterlockedCompareExchange((long *)dest_value, (long)new_value, (long)expected_old_value);
#endif
}

#endif