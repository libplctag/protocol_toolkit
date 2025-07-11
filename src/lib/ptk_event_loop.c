#include "ptk_event_loop_platform.h"

#if defined(PTK_PLATFORM_POSIX)
#include "platform/ptk_event_loop_platform_posix.c"
#elif defined(PTK_PLATFORM_WIN32)
// #include "platform/ptk_event_loop_platform_win32.c"
#elif defined(PTK_PLATFORM_FREERTOS)
// #include "platform/ptk_event_loop_platform_freertos.c"
#elif defined(PTK_PLATFORM_ZEPHYR)
// #include "platform/ptk_event_loop_platform_zephyr.c"
#endif

// Use ptk_event_loop_platform_* functions for event loop operations in core code.
