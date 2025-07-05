#pragma once

#include <stdint.h>

#include "ev_err.h"

#if defined(_WIN32) && defined(_MSC_VER)
/**
 * @brief Define __func__ as __FUNCTION__ for MSVC compatibility.
 */
#    define __func__ __FUNCTION__
#endif


typedef struct { uint32_t id; } shared_handle;
typedef struct { uint32_t id; } shared_stored_handle;


ev_err ev_shared_make(ev_shared_handle *handle, void *data, void (*destructor_func)(void *data));

ev_err ev_shared_store(ev_shared_stored_handle *dest_handle, ev_stored_handle src_handle);

ev_err ev_shared_release_stored(ev_shared_stored_handle *handle);

void *ev_shared_acquire_impl(const char *function_name, int line_number, handle.id);
#define ev_shared_acquire(handle) ev_shared_acquire_impl(const char *function_name, int line_number, handle.id)

void ev_shared_release_impl(const char *function_name, int line_number, handle.id);
#define ev_shared_release(handle) ev_shared_release_impl(const char *function_name, int line_number, handle.id)

#define \
