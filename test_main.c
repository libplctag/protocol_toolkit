#include "src/macos/include/protocol_toolkit.h"
#include <stdio.h>

int main(void) {
    printf("Testing Protocol Toolkit as submodule\n");

    // Test basic initialization
    ptk_transition_t transitions[1];
    ptk_transition_table_t tt;

    ptk_error_t result = ptk_tt_init(&tt, transitions, 1);
    if(result == PTK_SUCCESS) {
        printf("✓ Protocol Toolkit library linked successfully!\n");
        return 0;
    } else {
        printf("✗ Library initialization failed\n");
        return 1;
    }
}
