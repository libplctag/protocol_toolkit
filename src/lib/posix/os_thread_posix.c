// POSIX implementation of ptk_os_thread API
#include <ptk_os_thread.h>
#include <ptk_alloc.h>
#include <ptk_log.h>
#include <ptk_err.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>

//=============================================================================
// PLATFORM-SPECIFIC STRUCTURES
//=============================================================================

struct ptk_mutex {
    pthread_mutex_t mutex;
};

struct ptk_cond_var {
    pthread_cond_t cond;
};

struct ptk_thread {
    pthread_t thread;
};

//============================
// Internal Destructors
//============================

static void ptk_mutex_destructor(void *ptr) {
    ptk_mutex *m = (ptk_mutex *)ptr;
    if (m) {
        pthread_mutex_destroy(&m->mutex);
    }
}

static void ptk_cond_var_destructor(void *ptr) {
    ptk_cond_var *cv = (ptk_cond_var *)ptr;
    if (cv) {
        pthread_cond_destroy(&cv->cond);
    }
}

static void ptk_thread_destructor(void *ptr) {
    // No pthread resource to destroy after join, but placeholder for future
}

//============================
// Mutex Functions
//============================

ptk_mutex *ptk_mutex_create(void) {
    ptk_mutex *m = ptk_alloc(sizeof(ptk_mutex), ptk_mutex_destructor);
    if (!m) {
        error("ptk_mutex_create: allocation failed");
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return NULL;
    }
    pthread_mutexattr_t attr;
    if (pthread_mutexattr_init(&attr) != 0) {
        error("ptk_mutex_create: pthread_mutexattr_init failed");
        ptk_free(&m);
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return NULL;
    }
    if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) != 0) {
        error("ptk_mutex_create: pthread_mutexattr_settype failed");
        pthread_mutexattr_destroy(&attr);
        ptk_free(&m);
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return NULL;
    }
    if (pthread_mutex_init(&m->mutex, &attr) != 0) {
        error("ptk_mutex_create: pthread_mutex_init failed");
        pthread_mutexattr_destroy(&attr);
        ptk_free(&m);
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return NULL;
    }
    pthread_mutexattr_destroy(&attr);
    return m;
}

ptk_err ptk_mutex_wait_lock(ptk_mutex *mutex, ptk_time_ms timeout_ms) {
    if (!mutex) return PTK_ERR_INVALID_PARAM;
    // No timeout support for now
    int res = pthread_mutex_lock(&mutex->mutex);
    if (res != 0) {
        error("ptk_mutex_wait_lock: pthread_mutex_lock failed: %s", strerror(res));
        return PTK_ERR_CONFIGURATION_ERROR;
    }
    return PTK_OK;
}

ptk_err ptk_mutex_unlock(ptk_mutex *mutex) {
    if (!mutex) return PTK_ERR_INVALID_PARAM;
    int res = pthread_mutex_unlock(&mutex->mutex);
    if (res != 0) {
        error("ptk_mutex_unlock: pthread_mutex_unlock failed: %s", strerror(res));
        return PTK_ERR_CONFIGURATION_ERROR;
    }
    return PTK_OK;
}

//============================
// Condition Variable Functions
//============================

ptk_cond_var *ptk_cond_var_create(void) {
    ptk_cond_var *cv = ptk_alloc(sizeof(ptk_cond_var), ptk_cond_var_destructor);
    if (!cv) {
        error("ptk_cond_var_create: allocation failed");
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return NULL;
    }
    if (pthread_cond_init(&cv->cond, NULL) != 0) {
        error("ptk_cond_var_create: pthread_cond_init failed");
        ptk_free(&cv);
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return NULL;
    }
    return cv;
}

ptk_err ptk_cond_var_signal(ptk_cond_var *cond_var) {
    if (!cond_var) return PTK_ERR_INVALID_PARAM;
    int res = pthread_cond_signal(&cond_var->cond);
    if (res != 0) {
        error("ptk_cond_var_signal: pthread_cond_signal failed: %s", strerror(res));
        return PTK_ERR_CONFIGURATION_ERROR;
    }
    return PTK_OK;
}

ptk_err ptk_cond_var_wait(ptk_cond_var *cond_var, ptk_mutex *mutex, ptk_time_ms timeout_ms) {
    if (!cond_var || !mutex) return PTK_ERR_INVALID_PARAM;
    // No timeout support for now
    int res = pthread_cond_wait(&cond_var->cond, &mutex->mutex);
    if (res != 0) {
        error("ptk_cond_var_wait: pthread_cond_wait failed: %s", strerror(res));
        return PTK_ERR_CONFIGURATION_ERROR;
    }
    return PTK_OK;
}

//============================
// Thread Functions
//============================

static void *ptk_thread_entry(void *arg) {
    struct {
        ptk_thread_func func;
        void *data;
    } *info = arg;
    info->func(info->data);
    ptk_free(&info);
    return NULL;
}

ptk_thread *ptk_thread_create(ptk_thread_func func, void *data) {
    if (!func) return NULL;
    ptk_thread *t = ptk_alloc(sizeof(ptk_thread), ptk_thread_destructor);
    if (!t) {
        error("ptk_thread_create: allocation failed");
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return NULL;
    }
    // Allocate info struct for entry
    void *info = ptk_alloc(sizeof(ptk_thread_func) + sizeof(void *), NULL);
    if (!info) {
        error("ptk_thread_create: info allocation failed");
        ptk_free(&t);
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return NULL;
    }
    ((ptk_thread_func *)info)[0] = func;
    ((void **)info)[1] = data;
    int res = pthread_create(&t->thread, NULL, ptk_thread_entry, info);
    if (res != 0) {
        error("ptk_thread_create: pthread_create failed: %s", strerror(res));
        ptk_free(&info);
        ptk_free(&t);
        ptk_set_err(PTK_ERR_CONFIGURATION_ERROR);
        return NULL;
    }
    return t;
}

ptk_err ptk_thread_join(ptk_thread *thread) {
    if (!thread) return PTK_ERR_INVALID_PARAM;
    int res = pthread_join(thread->thread, NULL);
    if (res != 0) {
        error("ptk_thread_join: pthread_join failed: %s", strerror(res));
        return PTK_ERR_CONFIGURATION_ERROR;
    }
    ptk_free(&thread);
    return PTK_OK;
}
