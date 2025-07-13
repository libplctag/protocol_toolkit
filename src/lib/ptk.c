#include <ptk.h>
#include <ptk_shared.h>

void ptk_startup(void) {
    // Initialize shared memory system
    ptk_shared_init();
}

void ptk_shutdown(void) {
    // Shutdown shared memory system
    ptk_shared_shutdown();
}
