#include "ptk_atomic.h"

// Platform-specific atomic operations dispatcher
// This file includes the appropriate platform-specific implementation

#ifdef _WIN32
    #include "windows/atomic_operations.c"
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__APPLE__) || defined(__unix__)
    #include "posix/atomic_operations.c"
#else
    #error "Unsupported platform for atomic operations"
#endif