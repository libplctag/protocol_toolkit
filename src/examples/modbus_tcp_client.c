#include "protocol_toolkit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

// Define states and events for the state machine
enum client_states { STATE_INIT = 0, STATE_CONNECTING, STATE_SENDING_REQUEST, STATE_RECEIVING_RESPONSE, STATE_DISCONNECTED };
enum client_events { EVENT_CONNECT = 1, EVENT_SEND, EVENT_RECEIVE, EVENT_DISCONNECT };

// Static memory allocation
static ptk_tt_entry_t transitions[10];
static ptk_tt_t transition_table;
static ptk_tt_t *tables[1];
static ptk_ev_source_t *sources[3];
static ptk_ev_source_t connect_source, send_source, receive_source;
static ptk_sm_t state_machine;
static ptk_ev_t event_loop;
static ptk_socket_t client_socket;

// Action functions
void on_connect(ptk_sm_t *sm, ptk_ev_source_t *es, ptk_time_ms now_ms) {
    printf("Connecting to server...\n");
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(5020);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if(connect(client_socket.socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        sm->current_state = STATE_DISCONNECTED;
        return;
    }

    printf("Connected to server.\n");
    sm->current_state = STATE_SENDING_REQUEST;
}

void on_send_request(ptk_sm_t *sm, ptk_ev_source_t *es, ptk_time_ms now_ms) {
    printf("Sending Modbus request...\n");
    uint8_t request[12] = {0x01, 0x00, 0x00, 0x00, 0x0A};  // Example request
    ssize_t sent = send(client_socket.socket_fd, request, sizeof(request), 0);
    if(sent < 0) {
        perror("send");
        sm->current_state = STATE_DISCONNECTED;
        return;
    }

    printf("Request sent.\n");
    sm->current_state = STATE_RECEIVING_RESPONSE;
}

void on_receive_response(ptk_sm_t *sm, ptk_ev_source_t *es, ptk_time_ms now_ms) {
    printf("Receiving response...\n");
    uint8_t response[256];
    ssize_t received = recv(client_socket.socket_fd, response, sizeof(response), 0);
    if(received < 0) {
        perror("recv");
        sm->current_state = STATE_DISCONNECTED;
        return;
    }

    printf("Response received: %d bytes\n", (int)received);
    sm->current_state = STATE_DISCONNECTED;
}

int main() {
    printf("Starting Modbus TCP Client\n");

    // Initialize transition table
    ptk_tt_init(&transition_table, transitions, sizeof(transitions) / sizeof(transitions[0]));
    ptk_tt_add_transition(&transition_table, STATE_INIT, EVENT_CONNECT, STATE_CONNECTING, NULL, on_connect);
    ptk_tt_add_transition(&transition_table, STATE_CONNECTING, EVENT_SEND, STATE_SENDING_REQUEST, NULL, on_send_request);
    ptk_tt_add_transition(&transition_table, STATE_SENDING_REQUEST, EVENT_RECEIVE, STATE_RECEIVING_RESPONSE, NULL, on_receive_response);
    ptk_tt_add_transition(&transition_table, STATE_RECEIVING_RESPONSE, EVENT_DISCONNECT, STATE_DISCONNECTED, NULL, NULL);

    // Initialize state machine
    tables[0] = &transition_table;
    ptk_sm_init(&state_machine, tables, 1, sources, 3, &event_loop, NULL);
    ptk_sm_attach_table(&state_machine, &transition_table);

    // Initialize event loop
    ptk_loop_init(&event_loop, &state_machine);

    // Initialize event sources
    ptk_es_init_user_event(&connect_source, EVENT_CONNECT, NULL);
    ptk_es_init_user_event(&send_source, EVENT_SEND, NULL);
    ptk_es_init_user_event(&receive_source, EVENT_RECEIVE, NULL);

    // Attach event sources
    ptk_sm_attach_event_source(&state_machine, &connect_source);
    ptk_sm_attach_event_source(&state_machine, &send_source);
    ptk_sm_attach_event_source(&state_machine, &receive_source);

    // Trigger initial connection event
    ptk_sm_handle_event(&state_machine, EVENT_CONNECT, NULL, 0);

    // Run the event loop
    ptk_loop_run(&event_loop);

    printf("Client finished.\n");
    return 0;
}
