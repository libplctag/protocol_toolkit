/**
 * PTK - Protocol Toolkit Core Library
 * 
 * Main library initialization and thread-local error handling.
 */

#include "ptk.h"
#include <pthread.h>
#include <errno.h>
#include <string.h>

/* Thread-local storage for error codes */
static pthread_key_t error_key;
static pthread_once_t error_key_once = PTHREAD_ONCE_INIT;

/**
 * Initialize thread-local error storage key
 */
static void make_error_key(void) {
    (void)pthread_key_create(&error_key, NULL);
}

/**
 * Set thread-local error code (internal function)
 */
extern void ptk_set_error_internal(ptk_status_t error) {
    pthread_once(&error_key_once, make_error_key);
    pthread_setspecific(error_key, (void*)(intptr_t)error);
}

/**
 * Initialize the PTK library
 * Call this once before using any other PTK functions
 */
extern ptk_status_t ptk_init(void) {
    /* Initialize thread-local storage */
    pthread_once(&error_key_once, make_error_key);
    
    /* Clear any existing error */
    ptk_clear_error();
    
    return PTK_OK;
}

/**
 * Cleanup the PTK library
 * Call this to release any global resources
 */
extern void ptk_cleanup(void) {
    /* Currently no global resources to cleanup */
    /* pthread_key_delete(error_key) could be called here if needed */
}

/**
 * Get the last error for the current thread
 * PTK functions store errors in thread-local storage for composability
 */
extern ptk_status_t ptk_get_last_error(void) {
    pthread_once(&error_key_once, make_error_key);
    void* error_ptr = pthread_getspecific(error_key);
    return error_ptr ? (ptk_status_t)(intptr_t)error_ptr : PTK_OK;
}

/**
 * Clear the last error for the current thread
 */
extern void ptk_clear_error(void) {
    pthread_once(&error_key_once, make_error_key);
    pthread_setspecific(error_key, (void*)(intptr_t)PTK_OK);
}