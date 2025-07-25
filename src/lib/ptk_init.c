#include "ptk_types.h"
#include "ptk_log.h"
#include <pthread.h>
#include <stdbool.h>

static pthread_mutex_t ptk_error_mutex = PTHREAD_MUTEX_INITIALIZER;
static ptk_error_t ptk_last_error = PTK_OK;
static bool ptk_initialized = false;

ptk_status_t ptk_init(void) {
    if (ptk_initialized) return PTK_OK;
    ptk_initialized = true;
    PTK_LOG("PTK initialized");
    return PTK_OK;
}

void ptk_cleanup(void) {
    ptk_initialized = false;
    PTK_LOG("PTK cleaned up");
}

void ptk_set_last_error(ptk_error_t error_code) {
    pthread_mutex_lock(&ptk_error_mutex);
    ptk_last_error = error_code;
    pthread_mutex_unlock(&ptk_error_mutex);
}

ptk_error_t ptk_get_last_error(void) {
    pthread_mutex_lock(&ptk_error_mutex);
    ptk_error_t err = ptk_last_error;
    pthread_mutex_unlock(&ptk_error_mutex);
    return err;
}
