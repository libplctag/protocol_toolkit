/**
 * @file user_events.c
 * @brief macOS User Event Source Implementation using GCD
 */

#include "../../include/macos/protocol_toolkit.h"
#include <string.h>
#include <stdio.h>

/* External reference to global event loop slots */
extern ptk_event_loop_slot_t *g_event_loop_slots;
extern size_t g_num_slots;

/* ========================================================================
 * INTERNAL HELPERS
 * ======================================================================== */

/**
 * @brief Find user event source by handle
 */
static ptk_user_event_source_internal_t *find_user_event_source(ptk_handle_t handle) {
    if(handle == 0 || PTK_HANDLE_TYPE(handle) != PTK_TYPE_USER_EVENT_SOURCE) { return NULL; }

    uint8_t loop_id = PTK_HANDLE_EVENT_LOOP_ID(handle);
    if(loop_id >= g_num_slots) { return NULL; }

    ptk_event_loop_slot_t *slot = &g_event_loop_slots[loop_id];
    if(!slot->resources || !slot->resources->user_events) { return NULL; }

    /* Linear scan for exact handle match */
    for(size_t i = 0; i < slot->resources->num_user_events; i++) {
        ptk_user_event_source_internal_t *source = &slot->resources->user_events[i];
        if(source->base.handle == handle) { return source; }
    }

    return NULL;
}

/* ========================================================================
 * USER EVENT SOURCE MANAGEMENT
 * ======================================================================== */

ptk_handle_t ptk_user_event_source_create(ptk_handle_t event_loop) {
    if(event_loop == 0 || PTK_HANDLE_TYPE(event_loop) != PTK_TYPE_EVENT_LOOP) { return PTK_ERR_INVALID_HANDLE; }

    uint8_t loop_id = PTK_HANDLE_EVENT_LOOP_ID(event_loop);
    if(loop_id >= g_num_slots) { return PTK_ERR_INVALID_HANDLE; }

    ptk_event_loop_slot_t *slot = &g_event_loop_slots[loop_id];
    if(!slot->resources || !slot->resources->user_events) {
        ptk_set_last_error(event_loop, PTK_ERR_INVALID_HANDLE);
        return PTK_ERR_INVALID_HANDLE;
    }

    /* Find free user event source slot */
    for(size_t i = 0; i < slot->resources->num_user_events; i++) {
        ptk_user_event_source_internal_t *source = &slot->resources->user_events[i];
        if(source->base.handle == 0) {
            /* Create dispatch queue for user events */
            char queue_name[64];
            snprintf(queue_name, sizeof(queue_name), "ptk.user_events.%zu.%zu", (size_t)loop_id, i);

            source->event_queue = dispatch_queue_create(queue_name, DISPATCH_QUEUE_SERIAL);
            if(!source->event_queue) {
                ptk_set_last_error(event_loop, PTK_ERR_OUT_OF_MEMORY);
                return PTK_ERR_OUT_OF_MEMORY;
            }

            /* Create user event dispatch source */
            source->user_source = dispatch_source_create(DISPATCH_SOURCE_TYPE_DATA_ADD, 0, 0, slot->main_queue);
            if(!source->user_source) {
                dispatch_release(source->event_queue);
                ptk_set_last_error(event_loop, PTK_ERR_OUT_OF_MEMORY);
                return PTK_ERR_OUT_OF_MEMORY;
            }

            /* Initialize user event source */
            source->generation_counter++;
            source->base.handle = PTK_MAKE_HANDLE(PTK_TYPE_USER_EVENT_SOURCE, loop_id, source->generation_counter, i);
            source->base.event_loop = event_loop;

            /* Clear event handlers */
            memset(source->event_handlers, 0, sizeof(source->event_handlers));

            /* Set up dispatch source handler (will be configured when handlers are registered) */
            dispatch_resume(source->user_source);

            return source->base.handle;
        }
    }

    ptk_set_last_error(event_loop, PTK_ERR_OUT_OF_MEMORY);
    return PTK_ERR_OUT_OF_MEMORY;
}

ptk_err_t ptk_raise_event(ptk_handle_t event_source, ptk_event_type_t event_type, void *event_data) {
    ptk_user_event_source_internal_t *source = find_user_event_source(event_source);
    if(!source) { return PTK_ERR_INVALID_HANDLE; }

    /* Thread-safe event raising using GCD */
    dispatch_async(source->event_queue, ^{
        /* Call registered event handlers */
        for(size_t i = 0; i < sizeof(source->event_handlers) / sizeof(source->event_handlers[0]); i++) {
            ptk_event_handler_t *handler = &source->event_handlers[i];
            if(handler->is_active && handler->event_type == event_type && handler->handler) {
                handler->handler(source->base.handle, event_type, event_data, handler->user_data);
            }
        }
    });

    /* Signal the user source to wake up the event loop */
    dispatch_source_merge_data(source->user_source, 1);

    return PTK_ERR_OK;
}

ptk_err_t ptk_user_event_source_destroy(ptk_handle_t event_source) {
    ptk_user_event_source_internal_t *source = find_user_event_source(event_source);
    if(!source) { return PTK_ERR_INVALID_HANDLE; }

    /* Clean up GCD resources */
    if(source->user_source) {
        dispatch_source_cancel(source->user_source);
        dispatch_release(source->user_source);
        source->user_source = NULL;
    }

    if(source->event_queue) {
        dispatch_release(source->event_queue);
        source->event_queue = NULL;
    }

    /* Clear user event source data */
    memset(source, 0, sizeof(*source));

    return PTK_ERR_OK;
}
