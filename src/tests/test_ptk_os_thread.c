/**
 * @file test_ptk_os_thread.c
 * @brief Tests for ptk_os_thread API
 *
 * This file tests mutex and thread creation. Logging uses ptk_log.h, not ptk_os_thread.h except for the functions under test.
 */
#include <ptk_os_thread.h>
#include <ptk_log.h>
#include <stdio.h>

static int thread_ran = 0;

void thread_entry(void *param) {
    info("thread_entry called");
    thread_ran = 1;
}

int test_thread_create() {
    info("test_thread_create entry");
    thread_ran = 0;
    ptk_thread *th = ptk_thread_create(thread_entry, NULL);
    if(!th) {
        error("ptk_thread_create failed");
        return 1;
    }
    ptk_thread_join(th);
    if(!thread_ran) {
        error("thread did not run");
        return 2;
    }
    ptk_thread_destroy(th);
    info("test_thread_create exit");
    return 0;
}

int test_mutex() {
    info("test_mutex entry");
    ptk_mutex *mtx = ptk_mutex_create();
    if(!mtx) {
        error("ptk_mutex_create failed");
        return 1;
    }
    if(ptk_mutex_lock(mtx) != PTK_OK) {
        error("ptk_mutex_lock failed");
        ptk_mutex_destroy(mtx);
        return 2;
    }
    if(ptk_mutex_unlock(mtx) != PTK_OK) {
        error("ptk_mutex_unlock failed");
        ptk_mutex_destroy(mtx);
        return 3;
    }
    ptk_mutex_destroy(mtx);
    info("test_mutex exit");
    return 0;
}

int main() {
    int result = test_thread_create();
    if(result == 0) {
        info("ptk_os_thread thread test PASSED");
    } else {
        error("ptk_os_thread thread test FAILED");
        return result;
    }
    result = test_mutex();
    if(result == 0) {
        info("ptk_os_thread mutex test PASSED");
    } else {
        error("ptk_os_thread mutex test FAILED");
    }
    return result;
}
