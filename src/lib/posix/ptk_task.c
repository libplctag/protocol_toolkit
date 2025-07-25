#include "ptk_task.h"
#include "ptk_log.h"
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include <stdatomic.h>

// ================= ATOMIC OPERATIONS IMPLEMENTATION =================

// 8-bit signed
int8_t ptk_atomic_load_i8(const volatile int8_t* obj) {
    return atomic_load((_Atomic int8_t*)obj);
}
void ptk_atomic_store_i8(volatile int8_t* obj, int8_t val) {
    atomic_store((_Atomic int8_t*)obj, val);
}
int8_t ptk_atomic_add_i8(volatile int8_t* obj, int8_t val) {
    return atomic_fetch_add((_Atomic int8_t*)obj, val);
}
int8_t ptk_atomic_sub_i8(volatile int8_t* obj, int8_t val) {
    return atomic_fetch_sub((_Atomic int8_t*)obj, val);
}
int8_t ptk_atomic_and_i8(volatile int8_t* obj, int8_t val) {
    return atomic_fetch_and((_Atomic int8_t*)obj, val);
}
int8_t ptk_atomic_or_i8(volatile int8_t* obj, int8_t val) {
    return atomic_fetch_or((_Atomic int8_t*)obj, val);
}
int8_t ptk_atomic_xor_i8(volatile int8_t* obj, int8_t val) {
    return atomic_fetch_xor((_Atomic int8_t*)obj, val);
}
int8_t ptk_atomic_cas_i8(volatile int8_t* obj, int8_t expected, int8_t desired) {
    atomic_compare_exchange_strong((_Atomic int8_t*)obj, &expected, desired);
    return expected;
}

// 8-bit unsigned
uint8_t ptk_atomic_load_u8(const volatile uint8_t* obj) {
    return atomic_load((_Atomic uint8_t*)obj);
}
void ptk_atomic_store_u8(volatile uint8_t* obj, uint8_t val) {
    atomic_store((_Atomic uint8_t*)obj, val);
}
uint8_t ptk_atomic_add_u8(volatile uint8_t* obj, uint8_t val) {
    return atomic_fetch_add((_Atomic uint8_t*)obj, val);
}
uint8_t ptk_atomic_sub_u8(volatile uint8_t* obj, uint8_t val) {
    return atomic_fetch_sub((_Atomic uint8_t*)obj, val);
}
uint8_t ptk_atomic_and_u8(volatile uint8_t* obj, uint8_t val) {
    return atomic_fetch_and((_Atomic uint8_t*)obj, val);
}
uint8_t ptk_atomic_or_u8(volatile uint8_t* obj, uint8_t val) {
    return atomic_fetch_or((_Atomic uint8_t*)obj, val);
}
uint8_t ptk_atomic_xor_u8(volatile uint8_t* obj, uint8_t val) {
    return atomic_fetch_xor((_Atomic uint8_t*)obj, val);
}
uint8_t ptk_atomic_cas_u8(volatile uint8_t* obj, uint8_t expected, uint8_t desired) {
    atomic_compare_exchange_strong((_Atomic uint8_t*)obj, &expected, desired);
    return expected;
}

// 16-bit signed
int16_t ptk_atomic_load_i16(const volatile int16_t* obj) {
    return atomic_load((_Atomic int16_t*)obj);
}
void ptk_atomic_store_i16(volatile int16_t* obj, int16_t val) {
    atomic_store((_Atomic int16_t*)obj, val);
}
int16_t ptk_atomic_add_i16(volatile int16_t* obj, int16_t val) {
    return atomic_fetch_add((_Atomic int16_t*)obj, val);
}
int16_t ptk_atomic_sub_i16(volatile int16_t* obj, int16_t val) {
    return atomic_fetch_sub((_Atomic int16_t*)obj, val);
}
int16_t ptk_atomic_and_i16(volatile int16_t* obj, int16_t val) {
    return atomic_fetch_and((_Atomic int16_t*)obj, val);
}
int16_t ptk_atomic_or_i16(volatile int16_t* obj, int16_t val) {
    return atomic_fetch_or((_Atomic int16_t*)obj, val);
}
int16_t ptk_atomic_xor_i16(volatile int16_t* obj, int16_t val) {
    return atomic_fetch_xor((_Atomic int16_t*)obj, val);
}
int16_t ptk_atomic_cas_i16(volatile int16_t* obj, int16_t expected, int16_t desired) {
    atomic_compare_exchange_strong((_Atomic int16_t*)obj, &expected, desired);
    return expected;
}

// 16-bit unsigned
uint16_t ptk_atomic_load_u16(const volatile uint16_t* obj) {
    return atomic_load((_Atomic uint16_t*)obj);
}
void ptk_atomic_store_u16(volatile uint16_t* obj, uint16_t val) {
    atomic_store((_Atomic uint16_t*)obj, val);
}
uint16_t ptk_atomic_add_u16(volatile uint16_t* obj, uint16_t val) {
    return atomic_fetch_add((_Atomic uint16_t*)obj, val);
}
uint16_t ptk_atomic_sub_u16(volatile uint16_t* obj, uint16_t val) {
    return atomic_fetch_sub((_Atomic uint16_t*)obj, val);
}
uint16_t ptk_atomic_and_u16(volatile uint16_t* obj, uint16_t val) {
    return atomic_fetch_and((_Atomic uint16_t*)obj, val);
}
uint16_t ptk_atomic_or_u16(volatile uint16_t* obj, uint16_t val) {
    return atomic_fetch_or((_Atomic uint16_t*)obj, val);
}
uint16_t ptk_atomic_xor_u16(volatile uint16_t* obj, uint16_t val) {
    return atomic_fetch_xor((_Atomic uint16_t*)obj, val);
}
uint16_t ptk_atomic_cas_u16(volatile uint16_t* obj, uint16_t expected, uint16_t desired) {
    atomic_compare_exchange_strong((_Atomic uint16_t*)obj, &expected, desired);
    return expected;
}

// 32-bit signed
int32_t ptk_atomic_load_i32(const volatile int32_t* obj) {
    return atomic_load((_Atomic int32_t*)obj);
}
void ptk_atomic_store_i32(volatile int32_t* obj, int32_t val) {
    atomic_store((_Atomic int32_t*)obj, val);
}
int32_t ptk_atomic_add_i32(volatile int32_t* obj, int32_t val) {
    return atomic_fetch_add((_Atomic int32_t*)obj, val);
}
int32_t ptk_atomic_sub_i32(volatile int32_t* obj, int32_t val) {
    return atomic_fetch_sub((_Atomic int32_t*)obj, val);
}
int32_t ptk_atomic_and_i32(volatile int32_t* obj, int32_t val) {
    return atomic_fetch_and((_Atomic int32_t*)obj, val);
}
int32_t ptk_atomic_or_i32(volatile int32_t* obj, int32_t val) {
    return atomic_fetch_or((_Atomic int32_t*)obj, val);
}
int32_t ptk_atomic_xor_i32(volatile int32_t* obj, int32_t val) {
    return atomic_fetch_xor((_Atomic int32_t*)obj, val);
}
int32_t ptk_atomic_cas_i32(volatile int32_t* obj, int32_t expected, int32_t desired) {
    atomic_compare_exchange_strong((_Atomic int32_t*)obj, &expected, desired);
    return expected;
}

// 32-bit unsigned
uint32_t ptk_atomic_load_u32(const volatile uint32_t* obj) {
    return atomic_load((_Atomic uint32_t*)obj);
}
void ptk_atomic_store_u32(volatile uint32_t* obj, uint32_t val) {
    atomic_store((_Atomic uint32_t*)obj, val);
}
uint32_t ptk_atomic_add_u32(volatile uint32_t* obj, uint32_t val) {
    return atomic_fetch_add((_Atomic uint32_t*)obj, val);
}
uint32_t ptk_atomic_sub_u32(volatile uint32_t* obj, uint32_t val) {
    return atomic_fetch_sub((_Atomic uint32_t*)obj, val);
}
uint32_t ptk_atomic_and_u32(volatile uint32_t* obj, uint32_t val) {
    return atomic_fetch_and((_Atomic uint32_t*)obj, val);
}
uint32_t ptk_atomic_or_u32(volatile uint32_t* obj, uint32_t val) {
    return atomic_fetch_or((_Atomic uint32_t*)obj, val);
}
uint32_t ptk_atomic_xor_u32(volatile uint32_t* obj, uint32_t val) {
    return atomic_fetch_xor((_Atomic uint32_t*)obj, val);
}
uint32_t ptk_atomic_cas_u32(volatile uint32_t* obj, uint32_t expected, uint32_t desired) {
    atomic_compare_exchange_strong((_Atomic uint32_t*)obj, &expected, desired);
    return expected;
}

// 64-bit signed
int64_t ptk_atomic_load_i64(const volatile int64_t* obj) {
    return atomic_load((_Atomic int64_t*)obj);
}
void ptk_atomic_store_i64(volatile int64_t* obj, int64_t val) {
    atomic_store((_Atomic int64_t*)obj, val);
}
int64_t ptk_atomic_add_i64(volatile int64_t* obj, int64_t val) {
    return atomic_fetch_add((_Atomic int64_t*)obj, val);
}
int64_t ptk_atomic_sub_i64(volatile int64_t* obj, int64_t val) {
    return atomic_fetch_sub((_Atomic int64_t*)obj, val);
}
int64_t ptk_atomic_and_i64(volatile int64_t* obj, int64_t val) {
    return atomic_fetch_and((_Atomic int64_t*)obj, val);
}
int64_t ptk_atomic_or_i64(volatile int64_t* obj, int64_t val) {
    return atomic_fetch_or((_Atomic int64_t*)obj, val);
}
int64_t ptk_atomic_xor_i64(volatile int64_t* obj, int64_t val) {
    return atomic_fetch_xor((_Atomic int64_t*)obj, val);
}
int64_t ptk_atomic_cas_i64(volatile int64_t* obj, int64_t expected, int64_t desired) {
    atomic_compare_exchange_strong((_Atomic int64_t*)obj, &expected, desired);
    return expected;
}

// 64-bit unsigned
uint64_t ptk_atomic_load_u64(const volatile uint64_t* obj) {
    return atomic_load((_Atomic uint64_t*)obj);
}
void ptk_atomic_store_u64(volatile uint64_t* obj, uint64_t val) {
    atomic_store((_Atomic uint64_t*)obj, val);
}
uint64_t ptk_atomic_add_u64(volatile uint64_t* obj, uint64_t val) {
    return atomic_fetch_add((_Atomic uint64_t*)obj, val);
}
uint64_t ptk_atomic_sub_u64(volatile uint64_t* obj, uint64_t val) {
    return atomic_fetch_sub((_Atomic uint64_t*)obj, val);
}
uint64_t ptk_atomic_and_u64(volatile uint64_t* obj, uint64_t val) {
    return atomic_fetch_and((_Atomic uint64_t*)obj, val);
}
uint64_t ptk_atomic_or_u64(volatile uint64_t* obj, uint64_t val) {
    return atomic_fetch_or((_Atomic uint64_t*)obj, val);
}
uint64_t ptk_atomic_xor_u64(volatile uint64_t* obj, uint64_t val) {
    return atomic_fetch_xor((_Atomic uint64_t*)obj, val);
}
uint64_t ptk_atomic_cas_u64(volatile uint64_t* obj, uint64_t expected, uint64_t desired) {
    atomic_compare_exchange_strong((_Atomic uint64_t*)obj, &expected, desired);
    return expected;
}

// 32-bit float (bitwise atomic)
float ptk_atomic_load_f32(const volatile float* obj) {
    union { uint32_t u; float f; } val;
    val.u = atomic_load((_Atomic uint32_t*)obj);
    return val.f;
}
void ptk_atomic_store_f32(volatile float* obj, float valf) {
    union { uint32_t u; float f; } val = { .f = valf };
    atomic_store((_Atomic uint32_t*)obj, val.u);
}
float ptk_atomic_cas_f32(volatile float* obj, float expectedf, float desiredf) {
    union { uint32_t u; float f; } expected = { .f = expectedf }, desired = { .f = desiredf };
    atomic_compare_exchange_strong((_Atomic uint32_t*)obj, &expected.u, desired.u);
    return expected.f;
}

// 64-bit float (bitwise atomic)
double ptk_atomic_load_f64(const volatile double* obj) {
    union { uint64_t u; double d; } val;
    val.u = atomic_load((_Atomic uint64_t*)obj);
    return val.d;
}
void ptk_atomic_store_f64(volatile double* obj, double vald) {
    union { uint64_t u; double d; } val = { .d = vald };
    atomic_store((_Atomic uint64_t*)obj, val.u);
}
double ptk_atomic_cas_f64(volatile double* obj, double expectedd, double desiredd) {
    union { uint64_t u; double d; } expected = { .d = expectedd }, desired = { .d = desiredd };
    atomic_compare_exchange_strong((_Atomic uint64_t*)obj, &expected.u, desired.u);
    return expected.d;
}

// Pointer atomics (load, store, CAS only)
void* ptk_atomic_load_ptr(const volatile void** obj) {
    return atomic_load((_Atomic(void*)*)obj);
}
void ptk_atomic_store_ptr(volatile void** obj, void* val) {
    atomic_store((_Atomic(void*)*)obj, val);
}
void* ptk_atomic_cas_ptr(volatile void** obj, void* expected, void* desired) {
    atomic_compare_exchange_strong((_Atomic(void*)*)obj, &expected, desired);
    return expected;
}

// Internal helper to get pthread_t from ptk_task_t
static pthread_t* ptk_task_get_pthread(ptk_task_t* task) {
    return (pthread_t*)task->_platform;
}

// Internal wrapper to adapt user function to pthread signature
struct ptk_task_wrapper {
    ptk_task_fn fn;
    void* user_data;
};

static void* ptk_task_entry(void* arg) {
    struct ptk_task_wrapper wrapper = *(struct ptk_task_wrapper*)arg;
    free(arg);
    wrapper.fn(wrapper.user_data);
    return NULL;
}

ptk_status_t ptk_task_start(ptk_task_t* task, ptk_task_fn fn, void* user_data, const ptk_task_attr_t* attr) {
    if (!task || !fn) return PTK_ERROR_INVALID_PARAM;
    pthread_attr_t pattr;
    pthread_attr_t* pattr_ptr = NULL;
    int rc;
    if (attr) {
        rc = pthread_attr_init(&pattr);
        if (rc != 0) {
            PTK_LOG("pthread_attr_init failed: %s", strerror(rc));
            return PTK_ERROR_INVALID_PARAM;
        }
        if (attr->stack && attr->stack_size) {
            rc = pthread_attr_setstack(&pattr, attr->stack, attr->stack_size);
            if (rc != 0) {
                PTK_LOG("pthread_attr_setstack failed: %s", strerror(rc));
                pthread_attr_destroy(&pattr);
                return PTK_ERROR_INVALID_PARAM;
            }
        } else if (attr->stack_size) {
            rc = pthread_attr_setstacksize(&pattr, attr->stack_size);
            if (rc != 0) {
                PTK_LOG("pthread_attr_setstacksize failed: %s", strerror(rc));
                pthread_attr_destroy(&pattr);
                return PTK_ERROR_INVALID_PARAM;
            }
        }
        if (attr->detached) {
            rc = pthread_attr_setdetachstate(&pattr, PTHREAD_CREATE_DETACHED);
            if (rc != 0) {
                PTK_LOG("pthread_attr_setdetachstate failed: %s", strerror(rc));
                pthread_attr_destroy(&pattr);
                return PTK_ERROR_INVALID_PARAM;
            }
        }
        pattr_ptr = &pattr;
    }
    struct ptk_task_wrapper* wrapper = malloc(sizeof(struct ptk_task_wrapper));
    if (!wrapper) {
        if (attr) pthread_attr_destroy(&pattr);
        PTK_LOG("malloc failed for task wrapper");
        return PTK_ERROR_OUT_OF_MEMORY;
    }
    wrapper->fn = fn;
    wrapper->user_data = user_data;
    pthread_t* tid = ptk_task_get_pthread(task);
    rc = pthread_create(tid, pattr_ptr, ptk_task_entry, wrapper);
    if (attr) pthread_attr_destroy(&pattr);
    if (rc != 0) {
        PTK_LOG("pthread_create failed: %s", strerror(rc));
        free(wrapper);
        return PTK_ERROR_THREAD_CREATE;
    }
    return PTK_OK;
}

void ptk_task_exit(void) {
    pthread_exit(NULL);
}
