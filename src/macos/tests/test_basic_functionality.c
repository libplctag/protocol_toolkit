#include "../include/protocol_toolkit.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

// Test basic initialization functions
int test_transition_table_init() {
    ptk_transition_t transitions[5];
    ptk_transition_table_t tt;

    ptk_error_t result = ptk_tt_init(&tt, transitions, 5);
    assert(result == PTK_SUCCESS);
    assert(tt.transitions == transitions);
    assert(tt.max_transitions == 5);
    assert(tt.transition_count == 0);

    printf("✓ Transition table initialization test passed\n");
    return 1;
}

// Test adding transitions
int test_transition_add() {
    ptk_transition_t transitions[5];
    ptk_transition_table_t tt;

    ptk_tt_init(&tt, transitions, 5);

    ptk_error_t result = ptk_tt_add_transition(&tt, 0, 1, 1, NULL, NULL);
    assert(result == PTK_SUCCESS);
    assert(tt.transition_count == 1);
    assert(tt.transitions[0].initial_state == 0);
    assert(tt.transitions[0].event_id == 1);
    assert(tt.transitions[0].next_state == 1);

    printf("✓ Transition add test passed\n");
    return 1;
}

// Test event source initialization
int test_event_source_init() {
    ptk_event_source_t es;

    // Test timer initialization
    ptk_error_t result = ptk_es_init_timer(&es, 42, 1000, true, NULL);
    assert(result == PTK_SUCCESS);
    assert(es.event_id == 42);
    assert(es.interval_ms == 1000);
    assert(es.periodic == true);
    assert(es.macos.type == PTK_ES_TIMER);

    // Test user event initialization
    result = ptk_es_init_user_event(&es, 99, NULL);
    assert(result == PTK_SUCCESS);
    assert(es.event_id == 99);
    assert(es.macos.type == PTK_ES_USER);

    printf("✓ Event source initialization test passed\n");
    return 1;
}

// Test loop initialization
int test_loop_init() {
    ptk_loop_t loop;

    ptk_error_t result = ptk_loop_init(&loop, NULL);
    assert(result == PTK_SUCCESS);
    assert(loop.macos.kqueue_fd != -1);
    assert(loop.macos.running == false);
    assert(loop.macos.next_timer_id == 1);

    // Cleanup
    if(loop.macos.kqueue_fd != -1) { close(loop.macos.kqueue_fd); }

    printf("✓ Loop initialization test passed\n");
    return 1;
}

// Test state machine initialization
int test_state_machine_init() {
    ptk_state_machine_t sm;
    ptk_transition_table_t *tables[5];
    ptk_event_source_t *sources[10];
    ptk_loop_t loop;

    ptk_loop_init(&loop, NULL);

    ptk_error_t result = ptk_sm_init(&sm, tables, 5, sources, 10, &loop, NULL);
    assert(result == PTK_SUCCESS);
    assert(sm.tables == tables);
    assert(sm.max_tables == 5);
    assert(sm.table_count == 0);
    assert(sm.sources == sources);
    assert(sm.max_sources == 10);
    assert(sm.source_count == 0);
    assert(sm.loop == &loop);

    // Cleanup
    if(loop.macos.kqueue_fd != -1) { close(loop.macos.kqueue_fd); }

    printf("✓ State machine initialization test passed\n");
    return 1;
}

int main() {
    printf("Running basic functionality tests...\n");
    printf("===================================\n");

    int tests_passed = 0;

    tests_passed += test_transition_table_init();
    tests_passed += test_transition_add();
    tests_passed += test_event_source_init();
    tests_passed += test_loop_init();
    tests_passed += test_state_machine_init();

    printf("\n%d/5 tests passed\n", tests_passed);

    if(tests_passed == 5) {
        printf("All tests passed\n");
        return 0;
    } else {
        printf("Some tests failed\n");
        return 1;
    }
}
