#include <ptk_threadlet.h>
#include <ptk_sock.h>
#include "threadlet_scheduler.h"
#include "threadlet_core.h"
#include <ptk_log.h>
#include <ptk_os_thread.h>

static ptk_thread_local bool integration_initialized = false;

ptk_err ptk_linux_integration_init(void) {
    info("Initializing Linux integration");
    
    if (integration_initialized) {
        warn("Linux integration already initialized for this thread");
        return PTK_OK;
    }
    
    event_loop_t *loop = event_loop_create();
    if (!loop) {
        error("Failed to create event loop");
        return ptk_get_err();
    }
    
    integration_initialized = true;
    
    info("Linux integration initialized successfully");
    return PTK_OK;
}

ptk_err ptk_linux_integration_run(void) {
    info("Running Linux integration event loop");
    
    if (!integration_initialized) {
        warn("Linux integration not initialized");
        ptk_set_err(PTK_ERR_INVALID_STATE);
        return PTK_ERR_INVALID_STATE;
    }
    
    event_loop_t *loop = get_thread_local_event_loop();
    if (!loop) {
        error("No event loop available");
        ptk_set_err(PTK_ERR_INVALID_STATE);
        return PTK_ERR_INVALID_STATE;
    }
    
    return event_loop_run(loop);
}

ptk_err ptk_linux_integration_stop(void) {
    info("Stopping Linux integration");
    
    event_loop_t *loop = get_thread_local_event_loop();
    if (!loop) {
        warn("No event loop to stop");
        return PTK_OK;
    }
    
    return event_loop_stop(loop);
}