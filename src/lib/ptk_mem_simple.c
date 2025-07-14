/**
 * @file ptk_mem_simple.c 
 * @brief Simple implementation of memory management for Protocol Toolkit
 */

#include <ptk_mem.h>
#include <ptk_err.h>
#include <ptk_log.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

// Simple wrapper around malloc/free for now
void *ptk_local_alloc_impl(const char *file, int line, size_t size, void (*destructor)(void *ptr)) {
    void *ptr = malloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

void *ptk_local_realloc_impl(const char *file, int line, void *ptr, size_t new_size) {
    return realloc(ptr, new_size);
}

void ptk_local_free_impl(const char *file, int line, void **ptr_ref) {
    if (ptr_ref && *ptr_ref) {
        free(*ptr_ref);
        *ptr_ref = NULL;
    }
}

// Simple shared memory implementation with basic handle table
static pthread_mutex_t shared_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool shared_initialized = false;

#define MAX_SHARED_HANDLES 1024
typedef struct {
    uint64_t handle_value;
    void *ptr;
    int ref_count;
} shared_entry_t;

static shared_entry_t shared_table[MAX_SHARED_HANDLES];
static uint64_t next_handle = 1;

ptk_err ptk_shared_init(void) {
    pthread_mutex_lock(&shared_mutex);
    shared_initialized = true;
    memset(shared_table, 0, sizeof(shared_table));
    pthread_mutex_unlock(&shared_mutex);
    return PTK_OK;
}

ptk_err ptk_shared_shutdown(void) {
    pthread_mutex_lock(&shared_mutex);
    shared_initialized = false;
    memset(shared_table, 0, sizeof(shared_table));
    pthread_mutex_unlock(&shared_mutex);
    return PTK_OK;
}

ptk_shared_handle_t ptk_shared_create_impl(const char *file, int line, void *ptr) {
    ptk_shared_handle_t handle = PTK_SHARED_INVALID_HANDLE;
    
    pthread_mutex_lock(&shared_mutex);
    
    // Find empty slot
    for (int i = 0; i < MAX_SHARED_HANDLES; i++) {
        if (shared_table[i].handle_value == 0) {
            shared_table[i].handle_value = next_handle++;
            shared_table[i].ptr = ptr;
            shared_table[i].ref_count = 1;
            handle.value = shared_table[i].handle_value;
            break;
        }
    }
    
    pthread_mutex_unlock(&shared_mutex);
    return handle;
}

void *ptk_shared_acquire(ptk_shared_handle_t handle) {
    void *result = NULL;
    
    pthread_mutex_lock(&shared_mutex);
    
    // Find handle in table
    for (int i = 0; i < MAX_SHARED_HANDLES; i++) {
        if (shared_table[i].handle_value == handle.value) {
            result = shared_table[i].ptr;
            shared_table[i].ref_count++;
            break;
        }
    }
    
    pthread_mutex_unlock(&shared_mutex);
    return result;
}

ptk_err ptk_shared_release(ptk_shared_handle_t handle) {
    pthread_mutex_lock(&shared_mutex);
    
    // Find handle in table
    for (int i = 0; i < MAX_SHARED_HANDLES; i++) {
        if (shared_table[i].handle_value == handle.value) {
            shared_table[i].ref_count--;
            if (shared_table[i].ref_count <= 0) {
                // Free the memory and clear the entry
                free(shared_table[i].ptr);
                memset(&shared_table[i], 0, sizeof(shared_table[i]));
            }
            break;
        }
    }
    
    pthread_mutex_unlock(&shared_mutex);
    return PTK_OK;
}

ptk_err ptk_shared_realloc(ptk_shared_handle_t handle, size_t new_size) {
    return PTK_ERR_UNSUPPORTED;
}

void ptk_shared_free_impl(const char *file, int line, void **ptr_ref) {
    ptk_local_free_impl(file, line, ptr_ref);
}