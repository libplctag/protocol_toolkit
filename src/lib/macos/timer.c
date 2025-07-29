/**
 * @file timer.c
 * @brief macOS Timer Implementation using dispatch_source_t
 */

#include "../../include/macos/protocol_toolkit.h"
#include <stdio.h>
#include <string.h>

/* External reference to global event loop slots */
extern ptk_event_loop_slot_t *g_event_loop_slots;
extern size_t g_num_slots;

/* ========================================================================
 * INTERNAL HELPERS
 * ======================================================================== */

/**
 * @brief Find timer by handle
 */
static ptk_timer_internal_t *find_timer(ptk_handle_t handle) {
    if(handle == 0 || PTK_HANDLE_TYPE(handle) != PTK_TYPE_TIMER) { return NULL; }

    uint8_t loop_id = PTK_HANDLE_EVENT_LOOP_ID(handle);
    if(loop_id >= g_num_slots) { return NULL; }

    ptk_event_loop_slot_t *slot = &g_event_loop_slots[loop_id];
    if(!slot->resources || !slot->resources->timers) { return NULL; }

    /* Linear scan for exact handle match */
    for(size_t i = 0; i < slot->resources->num_timers; i++) {
        ptk_timer_internal_t *timer = &slot->resources->timers[i];
        if(timer->base.handle == handle) { return timer; }
    }

    return NULL;
}

/**
 * @brief Timer event handler for GCD timer source
 */
static void timer_event_handler(void *context) {
    ptk_timer_internal_t *timer = (ptk_timer_internal_t *)context;

    /* Call registered event handlers */
    for(size_t i = 0; i < sizeof(timer->event_handlers) / sizeof(timer->event_handlers[0]); i++) {
        ptk_event_handler_t *handler = &timer->event_handlers[i];
        if(handler->is_active && handler->event_type == PTK_EVENT_TIMER_EXPIRED && handler->handler) {
            handler->handler(timer->base.handle, PTK_EVENT_TIMER_EXPIRED, NULL, handler->user_data);
        }
    }

    /* Stop one-shot timers */
    if(!timer->is_repeating) {
        timer->is_running = false;
        dispatch_source_cancel(timer->timer_source);
    }
}

/* ========================================================================
 * TIMER MANAGEMENT
 * ======================================================================== */

ptk_handle_t ptk_timer_create(ptk_handle_t event_loop) {
    if(event_loop == 0 || PTK_HANDLE_TYPE(event_loop) != PTK_TYPE_EVENT_LOOP) { return PTK_ERR_INVALID_HANDLE; }

    uint8_t loop_id = PTK_HANDLE_EVENT_LOOP_ID(event_loop);
    if(loop_id >= g_num_slots) { return PTK_ERR_INVALID_HANDLE; }

    ptk_event_loop_slot_t *slot = &g_event_loop_slots[loop_id];
    if(!slot->resources || !slot->resources->timers) {
        ptk_set_last_error(event_loop, PTK_ERR_INVALID_HANDLE);
        return PTK_ERR_INVALID_HANDLE;
    }

    /* Find free timer slot */
    for(size_t i = 0; i < slot->resources->num_timers; i++) {
        ptk_timer_internal_t *timer = &slot->resources->timers[i];
        if(timer->base.handle == 0) {
            /* Initialize timer */
            timer->generation_counter++;
            timer->base.handle = PTK_MAKE_HANDLE(PTK_TYPE_TIMER, loop_id, timer->generation_counter, i);
            timer->base.event_loop = event_loop;
            timer->timer_source = NULL;
            timer->interval_ms = 0;
            timer->is_repeating = false;
            timer->is_running = false;

            /* Clear event handlers */
            memset(timer->event_handlers, 0, sizeof(timer->event_handlers));

            return timer->base.handle;
        }
    }

    ptk_set_last_error(event_loop, PTK_ERR_OUT_OF_MEMORY);
    return PTK_ERR_OUT_OF_MEMORY;
}

ptk_err_t ptk_timer_start(ptk_handle_t timer, uint64_t interval_ms, bool repeat) {
    ptk_timer_internal_t *timer_obj = find_timer(timer);
    if(!timer_obj) { return PTK_ERR_INVALID_HANDLE; }

    if(interval_ms == 0) {
        ptk_set_last_error(timer_obj->base.event_loop, PTK_ERR_INVALID_ARGUMENT);
        return PTK_ERR_INVALID_ARGUMENT;
    }

    /* Stop existing timer if running */
    if(timer_obj->is_running && timer_obj->timer_source) {
        dispatch_source_cancel(timer_obj->timer_source);
        dispatch_release(timer_obj->timer_source);
        timer_obj->timer_source = NULL;
    }

    /* Get event loop queue */
    uint8_t loop_id = PTK_HANDLE_EVENT_LOOP_ID(timer);
    ptk_event_loop_slot_t *slot = &g_event_loop_slots[loop_id];

    /* Create GCD timer source */
    timer_obj->timer_source = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, slot->main_queue);
    if(!timer_obj->timer_source) {
        ptk_set_last_error(timer_obj->base.event_loop, PTK_ERR_OUT_OF_MEMORY);
        return PTK_ERR_OUT_OF_MEMORY;
    }

    /* Set timer parameters */
    timer_obj->interval_ms = interval_ms;
    timer_obj->is_repeating = repeat;
    timer_obj->is_running = true;

    /* Configure timer */
    dispatch_time_t start_time = dispatch_time(DISPATCH_TIME_NOW, interval_ms * NSEC_PER_MSEC);
    uint64_t interval_ns = repeat ? (interval_ms * NSEC_PER_MSEC) : DISPATCH_TIME_FOREVER;
    dispatch_source_set_timer(timer_obj->timer_source, start_time, interval_ns, interval_ms * NSEC_PER_MSEC / 10);

    /* Set event handler */
    dispatch_source_set_event_handler_f(timer_obj->timer_source, timer_event_handler);
    dispatch_set_context(timer_obj->timer_source, timer_obj);

    /* Start timer */
    dispatch_resume(timer_obj->timer_source);

    return PTK_ERR_OK;
}

ptk_err_t ptk_timer_stop(ptk_handle_t timer) {
    ptk_timer_internal_t *timer_obj = find_timer(timer);
    if(!timer_obj) { return PTK_ERR_INVALID_HANDLE; }

    if(timer_obj->is_running && timer_obj->timer_source) {
        dispatch_source_cancel(timer_obj->timer_source);
        dispatch_release(timer_obj->timer_source);
        timer_obj->timer_source = NULL;
        timer_obj->is_running = false;
    }

    return PTK_ERR_OK;
}

ptk_err_t ptk_timer_destroy(ptk_handle_t timer) {
    ptk_timer_internal_t *timer_obj = find_timer(timer);
    if(!timer_obj) { return PTK_ERR_INVALID_HANDLE; }

    /* Stop timer if running */
    if(timer_obj->is_running && timer_obj->timer_source) {
        dispatch_source_cancel(timer_obj->timer_source);
        dispatch_release(timer_obj->timer_source);
        timer_obj->timer_source = NULL;
    }

    /* Clear timer data */
    memset(timer_obj, 0, sizeof(*timer_obj));

    return PTK_ERR_OK;
}
