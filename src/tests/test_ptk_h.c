#include "ptk.h"
#include <assert.h>

void test_ptk_h(void) {
    // Only inclusion and macro checks possible
    assert(PTK_VERSION_MAJOR >= 0);
}

int main(void) {
    test_ptk_h();
    return 0;
}
