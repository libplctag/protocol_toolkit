/**
 * @file event_loop.c
 * @brief macOS Event Loop Implementation using Grand Central Dispatch
 */

#include "../../include/macos/protocol_toolkit.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>

/* ========================================================================
 * INTERNAL HELPERS
 * ======================================================================== */

/**
 * @brief Find event loop slot by handle
 */
static ptk_event_loop_slot_t *find_event_loop_slot(ptk_event_loop_slot_t *slots, size_t num_slots, ptk_handle_t handle) {
    if(handle == 0 || PTK_HANDLE_TYPE(handle) != PTK_TYPE_EVENT_LOOP) { return NULL; }

    uint8_t loop_id = PTK_HANDLE_EVENT_LOOP_ID(handle);
    if(loop_id >= num_slots) { return NULL; }

    ptk_event_loop_slot_t *slot = &slots[loop_id];
    return (slot->handle == handle) ? slot : NULL;
}

/**
 * @brief Get global reference to event loop slots (stored in first created event loop)
 * This is a workaround since we need access to slots array from resource functions
 */
ptk_event_loop_slot_t *g_event_loop_slots = NULL;
size_t g_num_slots = 0;

/* ========================================================================
 * EVENT LOOP MANAGEMENT
 * ======================================================================== */

ptk_handle_t ptk_event_loop_create(ptk_event_loop_slot_t *slots, size_t num_slots, ptk_event_loop_resources_t *resources) {
    if(!slots || num_slots == 0 || !resources) { return PTK_ERR_INVALID_ARGUMENT; }

    /* Store global reference on first call */
    if(g_event_loop_slots == NULL) {
        g_event_loop_slots = slots;
        g_num_slots = num_slots;
    }

    /* Find unused slot */
    for(size_t i = 0; i < num_slots; i++) {
        ptk_event_loop_slot_t *slot = &slots[i];
        if(slot->handle == 0) {
            /* Create dispatch queue for this event loop */
            char queue_name[64];
            snprintf(queue_name, sizeof(queue_name), "ptk.eventloop.%zu", i);

            slot->main_queue = dispatch_queue_create(queue_name, DISPATCH_QUEUE_SERIAL);
            if(!slot->main_queue) { return PTK_ERR_OUT_OF_MEMORY; }

            slot->event_group = dispatch_group_create();
            if(!slot->event_group) {
                dispatch_release(slot->main_queue);
                return PTK_ERR_OUT_OF_MEMORY;
            }

            /* Initialize slot */
            uint8_t event_loop_id = (uint8_t)i;
            slot->generation_counter++;
            slot->handle = PTK_MAKE_HANDLE(PTK_TYPE_EVENT_LOOP, event_loop_id, slot->generation_counter, i);
            slot->resources = resources;
            slot->last_error = PTK_ERR_OK;
            slot->is_running = false;

            return slot->handle;
        }
    }

    return PTK_ERR_OUT_OF_MEMORY;
}

ptk_err_t ptk_event_loop_run(ptk_handle_t event_loop) {
    ptk_event_loop_slot_t *slot = find_event_loop_slot(g_event_loop_slots, g_num_slots, event_loop);
    if(!slot) { return PTK_ERR_INVALID_HANDLE; }

    /* Run the dispatch queue for a short time to process events */
    slot->is_running = true;

    /* Process events with a timeout to allow periodic returns */
    dispatch_time_t timeout = dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_MSEC); /* 10ms timeout */
    dispatch_group_wait(slot->event_group, timeout);

    /* Always return OK - timeout is expected for periodic operation */
    return PTK_ERR_OK;
}

ptk_err_t ptk_event_loop_destroy(ptk_handle_t event_loop) {
    ptk_event_loop_slot_t *slot = find_event_loop_slot(g_event_loop_slots, g_num_slots, event_loop);
    if(!slot) { return PTK_ERR_INVALID_HANDLE; }

    slot->is_running = false;

    /* Clean up GCD resources */
    if(slot->event_group) {
        dispatch_release(slot->event_group);
        slot->event_group = NULL;
    }

    if(slot->main_queue) {
        dispatch_release(slot->main_queue);
        slot->main_queue = NULL;
    }

    /* Mark slot as unused */
    slot->handle = 0;
    slot->resources = NULL;

    return PTK_ERR_OK;
}

/* ========================================================================
 * ERROR HANDLING
 * ======================================================================== */

ptk_err_t ptk_get_last_error(ptk_handle_t any_resource_handle) {
    if(any_resource_handle == 0) { return PTK_ERR_INVALID_HANDLE; }

    uint8_t loop_id = PTK_HANDLE_EVENT_LOOP_ID(any_resource_handle);
    if(loop_id >= g_num_slots) { return PTK_ERR_INVALID_HANDLE; }

    return g_event_loop_slots[loop_id].last_error;
}

void ptk_set_last_error(ptk_handle_t any_resource_handle, ptk_err_t error) {
    if(any_resource_handle == 0) { return; }

    uint8_t loop_id = PTK_HANDLE_EVENT_LOOP_ID(any_resource_handle);
    if(loop_id >= g_num_slots) { return; }

    g_event_loop_slots[loop_id].last_error = error;
}

const char *ptk_error_string(ptk_err_t error) {
    switch(error) {
        case PTK_ERR_OK: return "Success";
        case PTK_ERR_INVALID_HANDLE: return "Invalid or stale handle";
        case PTK_ERR_INVALID_ARGUMENT: return "Invalid function argument";
        case PTK_ERR_OUT_OF_MEMORY: return "No available resource slots";
        case PTK_ERR_NOT_SUPPORTED: return "Operation not supported";
        case PTK_ERR_NETWORK_ERROR: return "Network operation failed";
        case PTK_ERR_TIMEOUT: return "Operation timed out";
        case PTK_ERR_WOULD_BLOCK: return "Operation would block";
        case PTK_ERR_CONNECTION_REFUSED: return "Connection refused";
        case PTK_ERR_CONNECTION_RESET: return "Connection reset by peer";
        case PTK_ERR_NOT_CONNECTED: return "Socket not connected";
        case PTK_ERR_ALREADY_CONNECTED: return "Socket already connected";
        case PTK_ERR_ADDRESS_IN_USE: return "Address already in use";
        case PTK_ERR_NO_ROUTE: return "No route to host";
        case PTK_ERR_MESSAGE_TOO_LARGE: return "Message too large";
        case PTK_ERR_PROTOCOL_ERROR: return "Protocol error";
        default: return "Unknown error";
    }
}
