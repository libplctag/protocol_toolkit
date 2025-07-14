#include <ptk.h>
#include <ptk_mem.h>

ptk_err ptk_startup(void) {
    // Initialize shared memory system
    return ptk_shared_init();
}

ptk_err ptk_shutdown(void) {
    // Shutdown shared memory system
    return ptk_shared_shutdown();
}
