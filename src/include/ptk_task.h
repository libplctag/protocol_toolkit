#ifndef PTK_TASK_H
#define PTK_TASK_H

#include "ptk_types.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Task function signature.
 * Returns void for maximum portability.
 */
typedef void (*ptk_task_fn)(void* user_data);

// ================= ATOMIC OPERATIONS API =================

#include <stddef.h>
#include <stdint.h>

// 8, 16, 32, 64-bit signed/unsigned integer atomics
// All functions are memory_order_seq_cst

// 8-bit
int8_t   ptk_atomic_load_i8(const volatile int8_t* obj);
void     ptk_atomic_store_i8(volatile int8_t* obj, int8_t val);
int8_t   ptk_atomic_add_i8(volatile int8_t* obj, int8_t val);
int8_t   ptk_atomic_sub_i8(volatile int8_t* obj, int8_t val);
int8_t   ptk_atomic_and_i8(volatile int8_t* obj, int8_t val);
int8_t   ptk_atomic_or_i8(volatile int8_t* obj, int8_t val);
int8_t   ptk_atomic_xor_i8(volatile int8_t* obj, int8_t val);
int8_t   ptk_atomic_cas_i8(volatile int8_t* obj, int8_t expected, int8_t desired);

uint8_t  ptk_atomic_load_u8(const volatile uint8_t* obj);
void     ptk_atomic_store_u8(volatile uint8_t* obj, uint8_t val);
uint8_t  ptk_atomic_add_u8(volatile uint8_t* obj, uint8_t val);
uint8_t  ptk_atomic_sub_u8(volatile uint8_t* obj, uint8_t val);
uint8_t  ptk_atomic_and_u8(volatile uint8_t* obj, uint8_t val);
uint8_t  ptk_atomic_or_u8(volatile uint8_t* obj, uint8_t val);
uint8_t  ptk_atomic_xor_u8(volatile uint8_t* obj, uint8_t val);
uint8_t  ptk_atomic_cas_u8(volatile uint8_t* obj, uint8_t expected, uint8_t desired);

// 16-bit
int16_t  ptk_atomic_load_i16(const volatile int16_t* obj);
void     ptk_atomic_store_i16(volatile int16_t* obj, int16_t val);
int16_t  ptk_atomic_add_i16(volatile int16_t* obj, int16_t val);
int16_t  ptk_atomic_sub_i16(volatile int16_t* obj, int16_t val);
int16_t  ptk_atomic_and_i16(volatile int16_t* obj, int16_t val);
int16_t  ptk_atomic_or_i16(volatile int16_t* obj, int16_t val);
int16_t  ptk_atomic_xor_i16(volatile int16_t* obj, int16_t val);
int16_t  ptk_atomic_cas_i16(volatile int16_t* obj, int16_t expected, int16_t desired);

uint16_t ptk_atomic_load_u16(const volatile uint16_t* obj);
void     ptk_atomic_store_u16(volatile uint16_t* obj, uint16_t val);
uint16_t ptk_atomic_add_u16(volatile uint16_t* obj, uint16_t val);
uint16_t ptk_atomic_sub_u16(volatile uint16_t* obj, uint16_t val);
uint16_t ptk_atomic_and_u16(volatile uint16_t* obj, uint16_t val);
uint16_t ptk_atomic_or_u16(volatile uint16_t* obj, uint16_t val);
uint16_t ptk_atomic_xor_u16(volatile uint16_t* obj, uint16_t val);
uint16_t ptk_atomic_cas_u16(volatile uint16_t* obj, uint16_t expected, uint16_t desired);

// 32-bit
int32_t  ptk_atomic_load_i32(const volatile int32_t* obj);
void     ptk_atomic_store_i32(volatile int32_t* obj, int32_t val);
int32_t  ptk_atomic_add_i32(volatile int32_t* obj, int32_t val);
int32_t  ptk_atomic_sub_i32(volatile int32_t* obj, int32_t val);
int32_t  ptk_atomic_and_i32(volatile int32_t* obj, int32_t val);
int32_t  ptk_atomic_or_i32(volatile int32_t* obj, int32_t val);
int32_t  ptk_atomic_xor_i32(volatile int32_t* obj, int32_t val);
int32_t  ptk_atomic_cas_i32(volatile int32_t* obj, int32_t expected, int32_t desired);

uint32_t ptk_atomic_load_u32(const volatile uint32_t* obj);
void     ptk_atomic_store_u32(volatile uint32_t* obj, uint32_t val);
uint32_t ptk_atomic_add_u32(volatile uint32_t* obj, uint32_t val);
uint32_t ptk_atomic_sub_u32(volatile uint32_t* obj, uint32_t val);
uint32_t ptk_atomic_and_u32(volatile uint32_t* obj, uint32_t val);
uint32_t ptk_atomic_or_u32(volatile uint32_t* obj, uint32_t val);
uint32_t ptk_atomic_xor_u32(volatile uint32_t* obj, uint32_t val);
uint32_t ptk_atomic_cas_u32(volatile uint32_t* obj, uint32_t expected, uint32_t desired);

// 64-bit
int64_t  ptk_atomic_load_i64(const volatile int64_t* obj);
void     ptk_atomic_store_i64(volatile int64_t* obj, int64_t val);
int64_t  ptk_atomic_add_i64(volatile int64_t* obj, int64_t val);
int64_t  ptk_atomic_sub_i64(volatile int64_t* obj, int64_t val);
int64_t  ptk_atomic_and_i64(volatile int64_t* obj, int64_t val);
int64_t  ptk_atomic_or_i64(volatile int64_t* obj, int64_t val);
int64_t  ptk_atomic_xor_i64(volatile int64_t* obj, int64_t val);
int64_t  ptk_atomic_cas_i64(volatile int64_t* obj, int64_t expected, int64_t desired);

uint64_t ptk_atomic_load_u64(const volatile uint64_t* obj);
void     ptk_atomic_store_u64(volatile uint64_t* obj, uint64_t val);
uint64_t ptk_atomic_add_u64(volatile uint64_t* obj, uint64_t val);
uint64_t ptk_atomic_sub_u64(volatile uint64_t* obj, uint64_t val);
uint64_t ptk_atomic_and_u64(volatile uint64_t* obj, uint64_t val);
uint64_t ptk_atomic_or_u64(volatile uint64_t* obj, uint64_t val);
uint64_t ptk_atomic_xor_u64(volatile uint64_t* obj, uint64_t val);
uint64_t ptk_atomic_cas_u64(volatile uint64_t* obj, uint64_t expected, uint64_t desired);

// 32/64-bit float atomics (bitwise, not arithmetic)
float    ptk_atomic_load_f32(const volatile float* obj);
void     ptk_atomic_store_f32(volatile float* obj, float val);
float    ptk_atomic_cas_f32(volatile float* obj, float expected, float desired);

double   ptk_atomic_load_f64(const volatile double* obj);
void     ptk_atomic_store_f64(volatile double* obj, double val);
double   ptk_atomic_cas_f64(volatile double* obj, double expected, double desired);

// Pointer atomics (load, store, CAS only)
void*    ptk_atomic_load_ptr(const volatile void** obj);
void     ptk_atomic_store_ptr(volatile void** obj, void* val);
void*    ptk_atomic_cas_ptr(volatile void** obj, void* expected, void* desired);

/**
 * Opaque task/thread handle (user allocates this struct).
 * Size is large enough for all supported platforms.
 */
typedef struct {
    uint8_t _platform[32]; // Sufficient for pthread_t, HANDLE, or RTOS task struct
} ptk_task_t;

/**
 * Task attributes (user allocates and sets fields).
 * Allows for stack pointer/size, priority, name, etc.
 * Not all fields are used on all platforms.
 */
typedef struct {
    void* stack;         // Optional: user-provided stack (RTOS)
    size_t stack_size;   // Stack size in bytes
    int priority;        // Task priority (RTOS, ignored on POSIX)
    const char* name;    // Task name (optional)
    int detached;        // 1 = detached, 0 = joinable (ignored on most RTOS)
} ptk_task_attr_t;

/**
 * Start a new task/thread.
 * @param task Pointer to user-allocated task struct
 * @param fn   Function to run in the new thread/task
 * @param user_data User data pointer passed to fn
 * @param attr Optional pointer to task attributes (can be NULL for defaults)
 * @return PTK_OK on success, error code otherwise
 */
ptk_status_t ptk_task_start(ptk_task_t* task, ptk_task_fn fn, void* user_data, const ptk_task_attr_t* attr);

/**
 * Terminate the calling task/thread immediately.
 * This function does not return.
 * On platforms where explicit termination is not supported, it should return from the task function.
 */
void ptk_task_exit(void);

#ifdef __cplusplus
}
#endif

#endif /* PTK_TASK_H */
