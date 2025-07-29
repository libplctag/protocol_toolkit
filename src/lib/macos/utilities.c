/**
 * @file utilities.c
 * @brief macOS Utility Functions
 */

#include "../../include/macos/protocol_toolkit.h"

/* External reference to global event loop slots */
extern ptk_event_loop_slot_t *g_event_loop_slots;
extern size_t g_num_slots;

/* ========================================================================
 * UTILITY FUNCTIONS
 * ======================================================================== */

bool ptk_handle_is_valid(ptk_handle_t handle) {
    if(handle == 0) { return false; }

    uint8_t resource_type = PTK_HANDLE_TYPE(handle);
    uint8_t loop_id = PTK_HANDLE_EVENT_LOOP_ID(handle);

    /* Check basic validity */
    if(resource_type == PTK_TYPE_INVALID || loop_id >= g_num_slots) { return false; }

    /* For event loop handles, check if the slot matches */
    if(resource_type == PTK_TYPE_EVENT_LOOP) {
        if(loop_id >= g_num_slots) { return false; }
        return (g_event_loop_slots[loop_id].handle == handle);
    }

    /* For other resources, check if they exist in their respective arrays */
    ptk_event_loop_slot_t *slot = &g_event_loop_slots[loop_id];
    if(!slot->resources) { return false; }

    switch(resource_type) {
        case PTK_TYPE_TIMER:
            if(!slot->resources->timers) { return false; }
            for(size_t i = 0; i < slot->resources->num_timers; i++) {
                if(slot->resources->timers[i].base.handle == handle) { return true; }
            }
            break;

        case PTK_TYPE_SOCKET:
            if(!slot->resources->sockets) { return false; }
            for(size_t i = 0; i < slot->resources->num_sockets; i++) {
                if(slot->resources->sockets[i].base.handle == handle) { return true; }
            }
            break;

        case PTK_TYPE_USER_EVENT_SOURCE:
            if(!slot->resources->user_events) { return false; }
            for(size_t i = 0; i < slot->resources->num_user_events; i++) {
                if(slot->resources->user_events[i].base.handle == handle) { return true; }
            }
            break;

        default: return false;
    }

    return false;
}

ptk_resource_type_t ptk_handle_get_type(ptk_handle_t handle) {
    if(handle == 0) { return PTK_TYPE_INVALID; }

    return (ptk_resource_type_t)PTK_HANDLE_TYPE(handle);
}

ptk_handle_t ptk_get_owning_event_loop(ptk_handle_t resource_handle) {
    if(resource_handle == 0) { return PTK_ERR_INVALID_HANDLE; }

    uint8_t resource_type = PTK_HANDLE_TYPE(resource_handle);
    uint8_t loop_id = PTK_HANDLE_EVENT_LOOP_ID(resource_handle);

    if(loop_id >= g_num_slots) { return PTK_ERR_INVALID_HANDLE; }

    /* If this is already an event loop handle, return it */
    if(resource_type == PTK_TYPE_EVENT_LOOP) { return resource_handle; }

    /* Return the event loop handle */
    return g_event_loop_slots[loop_id].handle;
}
