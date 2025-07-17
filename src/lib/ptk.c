#include <ptk.h>
#include <ptk_mem.h>

ptk_err_t ptk_startup(void) {
    // Initialize shared memory system
    return ptk_shared_init();
}

ptk_err_t ptk_shutdown(void) {
    // Shutdown shared memory system
    return ptk_shared_shutdown();
}
