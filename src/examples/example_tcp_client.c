#include "../macos/include/protocol_toolkit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Example states for a simple TCP client
enum client_states { STATE_INIT = 0, STATE_CONNECTING, STATE_CONNECTED, STATE_DISCONNECTED };

// Example events
enum client_events { EVENT_CONNECT = 1, EVENT_SOCKET_READY, EVENT_DATA_RECEIVED, EVENT_TIMEOUT, EVENT_DISCONNECT };

// Static memory allocation - no dynamic allocation!
static ptk_transition_t transitions[10];
static ptk_transition_table_t transition_table;
static ptk_transition_table_t *tables[1];
static ptk_event_source_t *sources[5];
static ptk_event_source_t timer_source;
static ptk_state_machine_t state_machine;
static ptk_loop_t event_loop;
static ptk_socket_t client_socket;

// Action functions
void on_connect_start(ptk_state_machine_t *sm, ptk_event_source_t *es, ptk_time_ms now_ms) {
    printf("Starting connection at %u ms\n", now_ms);

    // Open TCP client socket
    ptk_error_t result = ptk_socket_open_tcp_client(&client_socket, "127.0.0.1", 8080, NULL);
    if(result != PTK_SUCCESS) {
        printf("Failed to open socket: %d\n", result);
        return;
    }

    printf("Socket opened, transitioning to CONNECTING state\n");
}

void on_connection_established(ptk_state_machine_t *sm, ptk_event_source_t *es, ptk_time_ms now_ms) {
    printf("Connection established at %u ms\n", now_ms);

    // Send initial data
    const char *message = "Hello, Server!";
    ptk_socket_send(&client_socket, message, strlen(message));
}

void on_data_received(ptk_state_machine_t *sm, ptk_event_source_t *es, ptk_time_ms now_ms) {
    printf("Data received at %u ms\n", now_ms);

    char buffer[1024];
    size_t received_len;
    ptk_error_t result = ptk_socket_receive(&client_socket, buffer, sizeof(buffer) - 1, &received_len);

    if(result == PTK_SUCCESS && received_len > 0) {
        buffer[received_len] = '\0';
        printf("Received: %s\n", buffer);
    }
}

void on_timeout(ptk_state_machine_t *sm, ptk_event_source_t *es, ptk_time_ms now_ms) {
    printf("Timeout occurred at %u ms\n", now_ms);
    ptk_loop_stop(&event_loop);
}

void on_disconnect(ptk_state_machine_t *sm, ptk_event_source_t *es, ptk_time_ms now_ms) {
    printf("Disconnecting at %u ms\n", now_ms);
    close(client_socket.socket_fd);
    ptk_loop_stop(&event_loop);
}

int main() {
    printf("Protocol Toolkit macOS Example\n");
    printf("==============================\n");

    // Initialize transition table
    ptk_error_t result = ptk_tt_init(&transition_table, transitions, sizeof(transitions) / sizeof(transitions[0]));
    if(result != PTK_SUCCESS) {
        printf("Failed to initialize transition table: %d\n", result);
        return 1;
    }

    // Add transitions
    ptk_tt_add_transition(&transition_table, STATE_INIT, EVENT_CONNECT, STATE_CONNECTING, NULL, on_connect_start);
    ptk_tt_add_transition(&transition_table, STATE_CONNECTING, EVENT_SOCKET_READY, STATE_CONNECTED, NULL,
                          on_connection_established);
    ptk_tt_add_transition(&transition_table, STATE_CONNECTED, EVENT_DATA_RECEIVED, STATE_CONNECTED, NULL, on_data_received);
    ptk_tt_add_transition(&transition_table, STATE_CONNECTED, EVENT_TIMEOUT, STATE_DISCONNECTED, NULL, on_timeout);
    ptk_tt_add_transition(&transition_table, STATE_CONNECTED, EVENT_DISCONNECT, STATE_DISCONNECTED, NULL, on_disconnect);

    // Initialize state machine
    tables[0] = &transition_table;
    result = ptk_sm_init(&state_machine, tables, 1, sources, 5, &event_loop, NULL);
    if(result != PTK_SUCCESS) {
        printf("Failed to initialize state machine: %d\n", result);
        return 1;
    }

    // Attach transition table
    result = ptk_sm_attach_table(&state_machine, &transition_table);
    if(result != PTK_SUCCESS) {
        printf("Failed to attach transition table: %d\n", result);
        return 1;
    }

    // Initialize event loop
    result = ptk_loop_init(&event_loop, &state_machine);
    if(result != PTK_SUCCESS) {
        printf("Failed to initialize event loop: %d\n", result);
        return 1;
    }

    // Initialize timer for timeout
    result = ptk_es_init_timer(&timer_source, EVENT_TIMEOUT, 10000, false, NULL);  // 10 second timeout
    if(result != PTK_SUCCESS) {
        printf("Failed to initialize timer: %d\n", result);
        return 1;
    }

    // Attach timer to state machine
    result = ptk_sm_attach_event_source(&state_machine, &timer_source);
    if(result != PTK_SUCCESS) {
        printf("Failed to attach timer: %d\n", result);
        return 1;
    }

    // Trigger initial connection event
    printf("Starting state machine...\n");
    ptk_sm_handle_event(&state_machine, EVENT_CONNECT, NULL, 0);

    // Run the event loop
    printf("Running event loop...\n");
    ptk_loop_run(&event_loop);

    printf("Event loop finished.\n");

    // Cleanup
    if(event_loop.macos.kqueue_fd != -1) { close(event_loop.macos.kqueue_fd); }

    return 0;
}
