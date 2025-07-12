#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PTK_EVENT_SOURCE_SOCKET,
    PTK_EVENT_SOURCE_TIMER,
    PTK_EVENT_SOURCE_SIGNAL,
} ptk_event_source_type;

typedef struct ptk_event_source {
    ptk_event_source_type type;
    void* platform_handle;
    // Optionally: callback, context, etc.
} ptk_event_source_t;

#ifdef __cplusplus
}
#endif
