#pragma once

#include <stdint.h>
#include <ptk_err.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ptk_event_loop_platform ptk_event_loop_platform_t;
typedef struct ptk_event_source ptk_event_source_t;

/**
 * Create a new platform event loop.
 */
ptk_event_loop_platform_t* ptk_event_loop_platform_create(void);

/**
 * Destroy a platform event loop.
 */
void ptk_event_loop_platform_destroy(ptk_event_loop_platform_t* loop);

/**
 * Register an event source (socket, timer, etc.) with the loop.
 */
ptk_err ptk_event_loop_platform_register(ptk_event_loop_platform_t* loop, ptk_event_source_t* source);

/**
 * Unregister an event source from the loop.
 */
ptk_err ptk_event_loop_platform_unregister(ptk_event_loop_platform_t* loop, ptk_event_source_t* source);

/**
 * Wait for events from registered sources.
 * Returns the number of sources that have events, 0 on timeout, or <0 for error.
 * sources_out is filled with pointers to the event sources with events.
 */
int ptk_event_loop_platform_wait(ptk_event_loop_platform_t* loop, ptk_event_source_t** sources_out, int max_sources, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
