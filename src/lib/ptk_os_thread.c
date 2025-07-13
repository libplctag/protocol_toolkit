
#include "ptk_os_thread.h"

#if defined(_WIN32)
#include "windows/os_thread_win.c"
#else
#include "posix/os_thread_posix.c"
#endif