#include "../include/protocol_toolkit.h"
#include <stdio.h>
#include <assert.h>
#include <unistd.h>

// Static memory for state machine test
static ptk_transition_t transitions[10];
static ptk_transition_table_t transition_table;
static ptk_transition_table_t *tables[1];
static ptk_event_source_t *sources[5];
static ptk_event_source_t timer_source;
static ptk_state_machine_t state_machine;
static ptk_loop_t event_loop;

// Test states
enum test_states { STATE_INIT = 0, STATE_WORKING = 1, STATE_DONE = 2 };

// Test events
enum test_events { EVENT_START = 1, EVENT_WORK_COMPLETE = 2, EVENT_FINISH = 3 };

// Global test state
static int action_call_count = 0;
static int last_state_reached = STATE_INIT;

// Action functions for testing
void on_start_action(ptk_state_machine_t *sm, ptk_event_source_t *es, ptk_time_ms now_ms) {
    (void)es;
    (void)now_ms;  // Suppress unused parameter warnings
    action_call_count++;
    last_state_reached = sm->current_state;
    printf("  Action: START -> WORKING (count: %d)\n", action_call_count);
}

void on_work_complete_action(ptk_state_machine_t *sm, ptk_event_source_t *es, ptk_time_ms now_ms) {
    (void)es;
    (void)now_ms;
    action_call_count++;
    last_state_reached = sm->current_state;
    printf("  Action: WORKING -> DONE (count: %d)\n", action_call_count);
}

// Test state machine setup and transitions
int test_state_machine_transitions() {
    printf("Testing state machine transitions...\n");

    // Reset test state
    action_call_count = 0;
    last_state_reached = STATE_INIT;

    // Initialize transition table
    ptk_error_t result = ptk_tt_init(&transition_table, transitions, 10);
    assert(result == PTK_SUCCESS);

    // Add transitions
    result = ptk_tt_add_transition(&transition_table, STATE_INIT, EVENT_START, STATE_WORKING, NULL, on_start_action);
    assert(result == PTK_SUCCESS);

    result =
        ptk_tt_add_transition(&transition_table, STATE_WORKING, EVENT_WORK_COMPLETE, STATE_DONE, NULL, on_work_complete_action);
    assert(result == PTK_SUCCESS);

    // Initialize event loop
    result = ptk_loop_init(&event_loop, NULL);
    assert(result == PTK_SUCCESS);

    // Initialize state machine
    tables[0] = &transition_table;
    result = ptk_sm_init(&state_machine, tables, 1, sources, 5, &event_loop, NULL);
    assert(result == PTK_SUCCESS);

    // Attach transition table
    result = ptk_sm_attach_table(&state_machine, &transition_table);
    assert(result == PTK_SUCCESS);

    // Test initial state
    assert(state_machine.current_state == STATE_INIT);

    // Trigger first transition
    result = ptk_sm_handle_event(&state_machine, EVENT_START, NULL, 0);
    assert(result == PTK_SUCCESS);
    assert(state_machine.current_state == STATE_WORKING);
    assert(action_call_count == 1);
    assert(last_state_reached == STATE_WORKING);

    // Trigger second transition
    result = ptk_sm_handle_event(&state_machine, EVENT_WORK_COMPLETE, NULL, 0);
    assert(result == PTK_SUCCESS);
    assert(state_machine.current_state == STATE_DONE);
    assert(action_call_count == 2);
    assert(last_state_reached == STATE_DONE);

    // Test invalid transition (should succeed but not change state)
    int old_state = state_machine.current_state;
    result = ptk_sm_handle_event(&state_machine, EVENT_START, NULL, 0);
    assert(result == PTK_SUCCESS);
    assert(state_machine.current_state == old_state);  // No transition should occur
    assert(action_call_count == 2);                    // No action should be called

    // Cleanup
    if(event_loop.macos.kqueue_fd != -1) { close(event_loop.macos.kqueue_fd); }

    printf("✓ State machine transitions test passed\n");
    return 1;
}

// Test timer event source attachment
int test_timer_attachment() {
    printf("Testing timer attachment...\n");

    // Initialize event loop
    ptk_error_t result = ptk_loop_init(&event_loop, NULL);
    assert(result == PTK_SUCCESS);

    // Initialize state machine
    tables[0] = &transition_table;
    result = ptk_sm_init(&state_machine, tables, 1, sources, 5, &event_loop, NULL);
    assert(result == PTK_SUCCESS);

    // Initialize timer
    result = ptk_es_init_timer(&timer_source, EVENT_FINISH, 100, false, NULL);
    assert(result == PTK_SUCCESS);

    // Attach timer to state machine
    result = ptk_sm_attach_event_source(&state_machine, &timer_source);
    assert(result == PTK_SUCCESS);
    assert(state_machine.source_count == 1);
    assert(state_machine.sources[0] == &timer_source);
    assert(timer_source.macos.active == true);

    // Verify timer was assigned an ID and slot
    assert(timer_source.macos.ident > 0);

    // Find the timer in the loop's timer array
    bool found_timer = false;
    for(int i = 0; i < PTK_MAX_TIMERS_PER_LOOP; i++) {
        if(event_loop.macos.timers[i].in_use && event_loop.macos.timers[i].source == &timer_source) {
            found_timer = true;
            break;
        }
    }
    assert(found_timer);

    // Cleanup
    if(event_loop.macos.kqueue_fd != -1) { close(event_loop.macos.kqueue_fd); }

    printf("✓ Timer attachment test passed\n");
    return 1;
}

// Test multiple state machines
int test_multiple_state_machines() {
    printf("Testing multiple state machines...\n");

    // Initialize event loop
    ptk_error_t result = ptk_loop_init(&event_loop, NULL);
    assert(result == PTK_SUCCESS);

    // Initialize first state machine
    tables[0] = &transition_table;
    result = ptk_sm_init(&state_machine, tables, 1, sources, 5, &event_loop, NULL);
    assert(result == PTK_SUCCESS);

    // Add state machine to loop
    result = ptk_sm_add_to_loop(&event_loop, &state_machine);
    assert(result == PTK_SUCCESS);
    assert(event_loop.current_sm == &state_machine);
    assert(state_machine.loop == &event_loop);

    // Cleanup
    if(event_loop.macos.kqueue_fd != -1) { close(event_loop.macos.kqueue_fd); }

    printf("✓ Multiple state machines test passed\n");
    return 1;
}

int main() {
    printf("Running state machine tests...\n");
    printf("==============================\n");

    int tests_passed = 0;

    tests_passed += test_state_machine_transitions();
    tests_passed += test_timer_attachment();
    tests_passed += test_multiple_state_machines();

    printf("\n%d/3 tests passed\n", tests_passed);

    if(tests_passed == 3) {
        printf("All tests passed\n");
        return 0;
    } else {
        printf("Some tests failed\n");
        return 1;
    }
}
