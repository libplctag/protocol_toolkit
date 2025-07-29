/**
 * @file event_handling.c
 * @brief macOS Event Handler Management
 */

#include "../../include/macos/protocol_toolkit.h"
#include <string.h>

/* External reference to global event loop slots */
extern ptk_event_loop_slot_t *g_event_loop_slots;
extern size_t g_num_slots;

/* ========================================================================
 * INTERNAL HELPERS
 * ======================================================================== */

/**
 * @brief Find event handler array for a resource
 */
static ptk_event_handler_t *find_event_handlers(ptk_handle_t resource, size_t *num_handlers) {
    if(resource == 0 || !num_handlers) { return NULL; }

    uint8_t resource_type = PTK_HANDLE_TYPE(resource);
    uint8_t loop_id = PTK_HANDLE_EVENT_LOOP_ID(resource);

    if(loop_id >= g_num_slots) { return NULL; }

    ptk_event_loop_slot_t *slot = &g_event_loop_slots[loop_id];
    if(!slot->resources) { return NULL; }

    switch(resource_type) {
        case PTK_TYPE_TIMER: {
            if(!slot->resources->timers) { return NULL; }
            for(size_t i = 0; i < slot->resources->num_timers; i++) {
                ptk_timer_internal_t *timer = &slot->resources->timers[i];
                if(timer->base.handle == resource) {
                    *num_handlers = sizeof(timer->event_handlers) / sizeof(timer->event_handlers[0]);
                    return timer->event_handlers;
                }
            }
            break;
        }

        case PTK_TYPE_SOCKET: {
            if(!slot->resources->sockets) { return NULL; }
            for(size_t i = 0; i < slot->resources->num_sockets; i++) {
                ptk_socket_internal_t *socket = &slot->resources->sockets[i];
                if(socket->base.handle == resource) {
                    *num_handlers = sizeof(socket->event_handlers) / sizeof(socket->event_handlers[0]);
                    return socket->event_handlers;
                }
            }
            break;
        }

        case PTK_TYPE_USER_EVENT_SOURCE: {
            if(!slot->resources->user_events) { return NULL; }
            for(size_t i = 0; i < slot->resources->num_user_events; i++) {
                ptk_user_event_source_internal_t *source = &slot->resources->user_events[i];
                if(source->base.handle == resource) {
                    *num_handlers = sizeof(source->event_handlers) / sizeof(source->event_handlers[0]);
                    return source->event_handlers;
                }
            }
            break;
        }

        default: return NULL;
    }

    return NULL;
}

/* ========================================================================
 * EVENT HANDLING
 * ======================================================================== */

ptk_err_t ptk_set_event_handler(ptk_handle_t resource, ptk_event_type_t event_type, ptk_event_handler_func_t handler,
                                void *user_data) {
    if(resource == 0 || !handler) { return PTK_ERR_INVALID_ARGUMENT; }

    size_t num_handlers;
    ptk_event_handler_t *handlers = find_event_handlers(resource, &num_handlers);
    if(!handlers) { return PTK_ERR_INVALID_HANDLE; }

    /* Look for existing handler for this event type */
    for(size_t i = 0; i < num_handlers; i++) {
        if(handlers[i].event_type == event_type && handlers[i].is_active) {
            /* Update existing handler */
            handlers[i].handler = handler;
            handlers[i].user_data = user_data;
            return PTK_ERR_OK;
        }
    }

    /* Find free handler slot */
    for(size_t i = 0; i < num_handlers; i++) {
        if(!handlers[i].is_active) {
            handlers[i].event_type = event_type;
            handlers[i].handler = handler;
            handlers[i].user_data = user_data;
            handlers[i].is_active = true;
            return PTK_ERR_OK;
        }
    }

    /* No free slots */
    ptk_set_last_error(resource, PTK_ERR_OUT_OF_MEMORY);
    return PTK_ERR_OUT_OF_MEMORY;
}

ptk_err_t ptk_remove_event_handler(ptk_handle_t resource, ptk_event_type_t event_type) {
    if(resource == 0) { return PTK_ERR_INVALID_ARGUMENT; }

    size_t num_handlers;
    ptk_event_handler_t *handlers = find_event_handlers(resource, &num_handlers);
    if(!handlers) { return PTK_ERR_INVALID_HANDLE; }

    /* Find and remove handler */
    for(size_t i = 0; i < num_handlers; i++) {
        if(handlers[i].event_type == event_type && handlers[i].is_active) {
            handlers[i].is_active = false;
            handlers[i].handler = NULL;
            handlers[i].user_data = NULL;
            return PTK_ERR_OK;
        }
    }

    /* Handler not found - not an error */
    return PTK_ERR_OK;
}
