#include "../include/protocol_toolkit.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <time.h>

// =============================================================================
// PRIVATE HELPER FUNCTIONS
// =============================================================================

// Get current time in milliseconds
static ptk_time_ms ptk_get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ptk_time_ms)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

// Convert milliseconds to absolute timespec
static void ptk_ms_to_timespec(ptk_time_ms ms, struct timespec *ts) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    ts->tv_sec = now.tv_sec + (ms / 1000);
    ts->tv_nsec = now.tv_nsec + ((ms % 1000) * 1000000);

    if(ts->tv_nsec >= 1000000000) {
        ts->tv_sec++;
        ts->tv_nsec -= 1000000000;
    }
}

// Find available timer slot
static int ptk_find_timer_slot(ptk_loop_t *loop) {
    for(int i = 0; i < PTK_MAX_TIMERS_PER_LOOP; i++) {
        if(!loop->macos.timers[i].in_use) { return i; }
    }
    return -1;
}

// Set socket to non-blocking mode
static ptk_error_t ptk_set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if(flags == -1) { return PTK_ERR_SOCKET_FAILURE; }

    if(fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) { return PTK_ERR_SOCKET_FAILURE; }

    return PTK_SUCCESS;
}

// Register socket events with kevent
static ptk_error_t ptk_register_socket_events(ptk_loop_t *loop, ptk_socket_t *socket, ptk_event_source_t *read_source,
                                              ptk_event_source_t *write_source) {
    if(!loop || !socket || loop->macos.kqueue_fd == -1) { return PTK_ERR_INVALID_ARG; }

    struct kevent changes[2];
    int nchanges = 0;

    // Register read events if read_source provided
    if(read_source && !socket->macos.registered_read) {
        EV_SET(&changes[nchanges], socket->socket_fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, read_source);
        nchanges++;
        socket->macos.registered_read = true;
        socket->macos.read_source = read_source;
        read_source->macos.type = PTK_ES_SOCKET;
        read_source->macos.ident = socket->socket_fd;
        read_source->macos.active = true;
    }

    // Register write events if write_source provided
    if(write_source && !socket->macos.registered_write) {
        EV_SET(&changes[nchanges], socket->socket_fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, write_source);
        nchanges++;
        socket->macos.registered_write = true;
        socket->macos.write_source = write_source;
        write_source->macos.type = PTK_ES_SOCKET;
        write_source->macos.ident = socket->socket_fd;
        write_source->macos.active = true;
    }

    if(nchanges > 0) {
        if(kevent(loop->macos.kqueue_fd, changes, nchanges, NULL, 0, NULL) == -1) { return PTK_ERR_SOCKET_FAILURE; }
    }

    return PTK_SUCCESS;
}

// Unregister socket events from kevent
static ptk_error_t ptk_unregister_socket_events(ptk_loop_t *loop, ptk_socket_t *socket) {
    if(!loop || !socket || loop->macos.kqueue_fd == -1) { return PTK_ERR_INVALID_ARG; }

    struct kevent changes[2];
    int nchanges = 0;

    if(socket->macos.registered_read) {
        EV_SET(&changes[nchanges], socket->socket_fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
        nchanges++;
        socket->macos.registered_read = false;
        socket->macos.read_source = NULL;
    }

    if(socket->macos.registered_write) {
        EV_SET(&changes[nchanges], socket->socket_fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
        nchanges++;
        socket->macos.registered_write = false;
        socket->macos.write_source = NULL;
    }

    if(nchanges > 0) { kevent(loop->macos.kqueue_fd, changes, nchanges, NULL, 0, NULL); }

    return PTK_SUCCESS;
}

// =============================================================================
// EVENT LOOP IMPLEMENTATION
// =============================================================================

ptk_error_t ptk_loop_init(ptk_loop_t *loop, ptk_state_machine_t *initial_sm) {
    if(!loop) { return PTK_ERR_INVALID_ARG; }

    // Initialize kqueue
    loop->macos.kqueue_fd = kqueue();
    if(loop->macos.kqueue_fd == -1) { return PTK_ERR_SOCKET_FAILURE; }

    // Initialize state
    loop->current_sm = initial_sm;
    loop->platform_data = NULL;
    loop->macos.running = false;
    loop->macos.next_timer_id = 1;

    // Initialize timer slots
    for(int i = 0; i < PTK_MAX_TIMERS_PER_LOOP; i++) {
        loop->macos.timers[i].in_use = false;
        loop->macos.timers[i].source = NULL;
    }

    return PTK_SUCCESS;
}

void ptk_loop_run(ptk_loop_t *loop) {
    if(!loop || loop->macos.kqueue_fd == -1) { return; }

    loop->macos.running = true;

    while(loop->macos.running) {
        // Calculate timeout for next timer
        struct timespec timeout = {1, 0};  // Default 1 second timeout
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);

        // Find the earliest timer
        bool have_timer = false;
        for(int i = 0; i < PTK_MAX_TIMERS_PER_LOOP; i++) {
            if(loop->macos.timers[i].in_use && loop->macos.timers[i].source) {
                ptk_event_source_t *es = loop->macos.timers[i].source;
                if(!have_timer || es->macos.next_fire.tv_sec < timeout.tv_sec
                   || (es->macos.next_fire.tv_sec == timeout.tv_sec && es->macos.next_fire.tv_nsec < timeout.tv_nsec)) {
                    timeout = es->macos.next_fire;
                    have_timer = true;
                }
            }
        }

        // Convert absolute time to relative timeout
        if(have_timer) {
            timeout.tv_sec -= now.tv_sec;
            timeout.tv_nsec -= now.tv_nsec;
            if(timeout.tv_nsec < 0) {
                timeout.tv_sec--;
                timeout.tv_nsec += 1000000000;
            }
            if(timeout.tv_sec < 0) {
                timeout.tv_sec = 0;
                timeout.tv_nsec = 0;
            }
        }

        // Wait for events
        int nevents = kevent(loop->macos.kqueue_fd, NULL, 0, loop->macos.events, PTK_MAX_KEVENTS, have_timer ? &timeout : NULL);

        ptk_time_ms current_time = ptk_get_time_ms();

        // Process kevent results
        for(int i = 0; i < nevents; i++) {
            struct kevent *kev = &loop->macos.events[i];
            ptk_event_source_t *es = (ptk_event_source_t *)kev->udata;

            // Route event to the state machine that owns this event source
            if(es && es->macos.owner_sm) { ptk_sm_handle_event(es->macos.owner_sm, es->event_id, es, current_time); }
        }

        // Process expired timers
        clock_gettime(CLOCK_MONOTONIC, &now);
        for(int i = 0; i < PTK_MAX_TIMERS_PER_LOOP; i++) {
            if(loop->macos.timers[i].in_use && loop->macos.timers[i].source) {
                ptk_event_source_t *es = loop->macos.timers[i].source;

                if(now.tv_sec > es->macos.next_fire.tv_sec
                   || (now.tv_sec == es->macos.next_fire.tv_sec && now.tv_nsec >= es->macos.next_fire.tv_nsec)) {

                    // Timer expired - route to owning state machine
                    if(es->macos.owner_sm) { ptk_sm_handle_event(es->macos.owner_sm, es->event_id, es, current_time); }

                    // Handle periodic timers
                    if(es->periodic) {
                        ptk_ms_to_timespec(es->interval_ms, &es->macos.next_fire);
                    } else {
                        // One-shot timer, deactivate
                        loop->macos.timers[i].in_use = false;
                        es->macos.active = false;
                    }
                }
            }
        }
    }
}

void ptk_loop_stop(ptk_loop_t *loop) {
    if(loop) { loop->macos.running = false; }
}

// =============================================================================
// EVENT SOURCE IMPLEMENTATION
// =============================================================================

ptk_error_t ptk_es_init_timer(ptk_event_source_t *es, int event_id, ptk_time_ms interval_ms, bool periodic, void *user_data) {
    if(!es || interval_ms == 0) { return PTK_ERR_INVALID_ARG; }

    es->event_id = event_id;
    es->interval_ms = interval_ms;
    es->periodic = periodic;
    es->user_data = user_data;

    // Initialize macOS-specific data
    es->macos.type = PTK_ES_TIMER;
    es->macos.ident = 0;  // Will be set when attached to loop
    es->macos.active = false;

    return PTK_SUCCESS;
}

ptk_error_t ptk_es_init_user_event(ptk_event_source_t *es, int event_id, void *user_data) {
    if(!es) { return PTK_ERR_INVALID_ARG; }

    es->event_id = event_id;
    es->interval_ms = 0;
    es->periodic = false;
    es->user_data = user_data;

    // Initialize macOS-specific data
    es->macos.type = PTK_ES_USER;
    es->macos.ident = 0;
    es->macos.active = false;

    return PTK_SUCCESS;
}

// =============================================================================
// SOCKET IMPLEMENTATION
// =============================================================================

ptk_error_t ptk_socket_open_tcp_client(ptk_socket_t *sock, const char *remote_ip, uint16_t remote_port, void *user_data) {
    if(!sock || !remote_ip) { return PTK_ERR_INVALID_ARG; }

    // Create socket
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd == -1) { return PTK_ERR_SOCKET_FAILURE; }

    // Set non-blocking
    if(ptk_set_nonblocking(fd) != PTK_SUCCESS) {
        close(fd);
        return PTK_ERR_SOCKET_FAILURE;
    }

    // Setup address
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(remote_port);
    if(inet_pton(AF_INET, remote_ip, &addr.sin_addr) <= 0) {
        close(fd);
        return PTK_ERR_INVALID_ARG;
    }

    // Connect (will return EINPROGRESS for non-blocking)
    if(connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        if(errno != EINPROGRESS) {
            close(fd);
            return PTK_ERR_SOCKET_FAILURE;
        }
    }

    // Initialize socket structure
    sock->type = PTK_SOCKET_TCP;
    sock->socket_fd = fd;
    sock->user_data = user_data;
    sock->macos.nonblocking = true;
    sock->macos.registered_read = false;
    sock->macos.registered_write = false;
    sock->macos.read_source = NULL;
    sock->macos.write_source = NULL;

    return PTK_SUCCESS;
}

ptk_error_t ptk_socket_open_tcp_server(ptk_socket_t *sock, const char *local_ip, uint16_t local_port, void *user_data) {
    if(!sock) { return PTK_ERR_INVALID_ARG; }

    // Create socket
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd == -1) { return PTK_ERR_SOCKET_FAILURE; }

    // Set socket options
    int opt = 1;
    if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        close(fd);
        return PTK_ERR_SOCKET_FAILURE;
    }

    // Set non-blocking
    if(ptk_set_nonblocking(fd) != PTK_SUCCESS) {
        close(fd);
        return PTK_ERR_SOCKET_FAILURE;
    }

    // Setup address
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(local_port);

    if(local_ip) {
        if(inet_pton(AF_INET, local_ip, &addr.sin_addr) <= 0) {
            close(fd);
            return PTK_ERR_INVALID_ARG;
        }
    } else {
        addr.sin_addr.s_addr = INADDR_ANY;
    }

    // Bind and listen
    if(bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        close(fd);
        return PTK_ERR_SOCKET_FAILURE;
    }

    if(listen(fd, 10) == -1) {
        close(fd);
        return PTK_ERR_SOCKET_FAILURE;
    }

    // Initialize socket structure
    sock->type = PTK_SOCKET_TCP;
    sock->socket_fd = fd;
    sock->user_data = user_data;
    sock->macos.nonblocking = true;
    sock->macos.registered_read = false;
    sock->macos.registered_write = false;
    sock->macos.read_source = NULL;
    sock->macos.write_source = NULL;

    return PTK_SUCCESS;
}

ptk_error_t ptk_socket_open_udp(ptk_socket_t *sock, const char *local_ip, uint16_t local_port, void *user_data) {
    if(!sock) { return PTK_ERR_INVALID_ARG; }

    // Create socket
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(fd == -1) { return PTK_ERR_SOCKET_FAILURE; }

    // Set non-blocking
    if(ptk_set_nonblocking(fd) != PTK_SUCCESS) {
        close(fd);
        return PTK_ERR_SOCKET_FAILURE;
    }

    // Setup address
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(local_port);

    if(local_ip) {
        if(inet_pton(AF_INET, local_ip, &addr.sin_addr) <= 0) {
            close(fd);
            return PTK_ERR_INVALID_ARG;
        }
    } else {
        addr.sin_addr.s_addr = INADDR_ANY;
    }

    // Bind
    if(bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        close(fd);
        return PTK_ERR_SOCKET_FAILURE;
    }

    // Initialize socket structure
    sock->type = PTK_SOCKET_UDP;
    sock->socket_fd = fd;
    sock->user_data = user_data;
    sock->macos.nonblocking = true;
    sock->macos.registered_read = false;
    sock->macos.registered_write = false;
    sock->macos.read_source = NULL;
    sock->macos.write_source = NULL;

    return PTK_SUCCESS;
}

ptk_error_t ptk_socket_send(ptk_socket_t *socket, const void *data, size_t data_len) {
    if(!socket || !data || socket->socket_fd == -1) { return PTK_ERR_INVALID_ARG; }

    ssize_t sent = send(socket->socket_fd, data, data_len, 0);
    if(sent == -1) {
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            // Would block, this is normal for non-blocking sockets
            return PTK_SUCCESS;  // Or could return a specific "would block" error
        }
        return PTK_ERR_SOCKET_FAILURE;
    }

    // Note: In a real implementation, you'd want to handle partial sends
    // and potentially register for write events if the send would block

    return PTK_SUCCESS;
}

ptk_error_t ptk_socket_receive(ptk_socket_t *socket, void *buffer, size_t buffer_len, size_t *received_len) {
    if(!socket || !buffer || !received_len || socket->socket_fd == -1) { return PTK_ERR_INVALID_ARG; }

    ssize_t received = recv(socket->socket_fd, buffer, buffer_len, 0);
    if(received == -1) {
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            *received_len = 0;
            return PTK_SUCCESS;
        }
        return PTK_ERR_SOCKET_FAILURE;
    }

    *received_len = (size_t)received;
    return PTK_SUCCESS;
}

// =============================================================================
// STATE MACHINE IMPLEMENTATION STUBS
// =============================================================================

ptk_error_t ptk_sm_init(ptk_state_machine_t *sm, ptk_transition_table_t **tables, uint32_t max_tables,
                        ptk_event_source_t **sources, uint32_t max_sources, struct ptk_loop *loop, void *user_data) {
    if(!sm || !tables || !sources) { return PTK_ERR_INVALID_ARG; }

    sm->current_state = 0;
    sm->tables = tables;
    sm->table_count = 0;
    sm->max_tables = max_tables;
    sm->sources = sources;
    sm->source_count = 0;
    sm->max_sources = max_sources;
    sm->loop = loop;
    sm->user_data = user_data;

    return PTK_SUCCESS;
}

ptk_error_t ptk_sm_attach_event_source(ptk_state_machine_t *sm, ptk_event_source_t *es) {
    if(!sm || !es || sm->source_count >= sm->max_sources) { return PTK_ERR_INVALID_ARG; }

    // Set the owner state machine
    es->macos.owner_sm = sm;

    // Handle timer event sources
    if(es->macos.type == PTK_ES_TIMER && sm->loop) {
        // Find available timer slot
        int slot = ptk_find_timer_slot(sm->loop);
        if(slot == -1) { return PTK_ERR_OUT_OF_BOUNDS; }

        // Assign timer ID and activate
        es->macos.ident = sm->loop->macos.next_timer_id++;
        sm->loop->macos.timers[slot].in_use = true;
        sm->loop->macos.timers[slot].source = es;
        es->macos.active = true;

        // Set initial fire time
        ptk_ms_to_timespec(es->interval_ms, &es->macos.next_fire);
    }

    sm->sources[sm->source_count++] = es;
    return PTK_SUCCESS;
}

ptk_error_t ptk_sm_handle_event(ptk_state_machine_t *sm, int event_id, ptk_event_source_t *es, ptk_time_ms now_ms) {
    if(!sm) { return PTK_ERR_INVALID_ARG; }

    // Search through transition tables for matching transition
    for(uint32_t i = 0; i < sm->table_count; i++) {
        ptk_transition_table_t *tt = sm->tables[i];
        for(uint32_t j = 0; j < tt->transition_count; j++) {
            ptk_transition_t *trans = &tt->transitions[j];

            if(trans->initial_state == sm->current_state && trans->event_id == event_id) {
                // Execute action if present
                if(trans->action) { trans->action(sm, es, now_ms); }

                // Transition to next state
                sm->current_state = trans->next_state;

                // Handle state machine transitions
                if(trans->next_sm) {
                    if(sm->loop) { sm->loop->current_sm = trans->next_sm; }
                }

                return PTK_SUCCESS;
            }
        }
    }

    return PTK_SUCCESS;  // No matching transition found, but not an error
}

// =============================================================================
// TRANSITION TABLE IMPLEMENTATION STUBS
// =============================================================================

ptk_error_t ptk_tt_init(ptk_transition_table_t *tt, ptk_transition_t *transitions, uint32_t max_transitions) {
    if(!tt || !transitions) { return PTK_ERR_INVALID_ARG; }

    tt->transitions = transitions;
    tt->transition_count = 0;
    tt->max_transitions = max_transitions;

    return PTK_SUCCESS;
}

ptk_error_t ptk_tt_add_transition(ptk_transition_table_t *tt, int initial_state, int event_id, int next_state,
                                  ptk_state_machine_t *next_sm, ptk_action_func action) {
    if(!tt || tt->transition_count >= tt->max_transitions) { return PTK_ERR_INVALID_ARG; }

    ptk_transition_t *trans = &tt->transitions[tt->transition_count];
    trans->initial_state = initial_state;
    trans->event_id = event_id;
    trans->next_state = next_state;
    trans->next_sm = next_sm;
    trans->action = action;

    tt->transition_count++;
    return PTK_SUCCESS;
}

// =============================================================================
// ADDITIONAL SOCKET FUNCTIONS (STUBS)
// =============================================================================

ptk_error_t ptk_socket_accept(ptk_socket_t *server_socket, ptk_socket_t *client_socket) {
    if(!server_socket || !client_socket || server_socket->socket_fd == -1) { return PTK_ERR_INVALID_ARG; }

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    int client_fd = accept(server_socket->socket_fd, (struct sockaddr *)&client_addr, &client_len);
    if(client_fd == -1) {
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            return PTK_ERR_SOCKET_FAILURE;  // No pending connections
        }
        return PTK_ERR_SOCKET_FAILURE;
    }

    // Set client socket to non-blocking
    if(ptk_set_nonblocking(client_fd) != PTK_SUCCESS) {
        close(client_fd);
        return PTK_ERR_SOCKET_FAILURE;
    }

    // Initialize client socket structure
    client_socket->type = PTK_SOCKET_TCP;
    client_socket->socket_fd = client_fd;
    client_socket->user_data = NULL;
    client_socket->macos.nonblocking = true;
    client_socket->macos.registered_read = false;
    client_socket->macos.registered_write = false;
    client_socket->macos.read_source = NULL;
    client_socket->macos.write_source = NULL;

    return PTK_SUCCESS;
}

ptk_error_t ptk_socket_receive_from(ptk_socket_t *socket, void *buffer, size_t buffer_len, size_t *received_len, char *sender_ip,
                                    uint16_t *sender_port) {
    // Implementation would use recvfrom() for UDP sockets
    return PTK_ERR_UNKNOWN;  // Stub
}

ptk_error_t ptk_socket_send_to(ptk_socket_t *socket, const char *target_ip, uint16_t target_port, const void *data,
                               size_t data_len) {
    // Implementation would use sendto() for UDP sockets
    return PTK_ERR_UNKNOWN;  // Stub
}

ptk_error_t ptk_socket_attach_multicast(ptk_socket_t *socket, const char *multicast_group, const char *local_ip) {
    // Implementation would use setsockopt() with IP_ADD_MEMBERSHIP
    return PTK_ERR_UNKNOWN;  // Stub
}

ptk_error_t ptk_socket_detach_multicast(ptk_socket_t *socket, const char *multicast_group, const char *local_ip) {
    // Implementation would use setsockopt() with IP_DROP_MEMBERSHIP
    return PTK_ERR_UNKNOWN;  // Stub
}

ptk_error_t ptk_sm_attach_table(ptk_state_machine_t *sm, ptk_transition_table_t *tt) {
    if(!sm || !tt || sm->table_count >= sm->max_tables) { return PTK_ERR_INVALID_ARG; }

    sm->tables[sm->table_count++] = tt;
    return PTK_SUCCESS;
}

ptk_error_t ptk_sm_add_to_loop(ptk_loop_t *loop, ptk_state_machine_t *sm) {
    if(!loop || !sm) { return PTK_ERR_INVALID_ARG; }

    loop->current_sm = sm;
    sm->loop = loop;
    return PTK_SUCCESS;
}

// Public socket event registration functions
ptk_error_t ptk_socket_register_events(ptk_loop_t *loop, ptk_socket_t *socket, ptk_event_source_t *read_source,
                                       ptk_event_source_t *write_source) {
    return ptk_register_socket_events(loop, socket, read_source, write_source);
}

ptk_error_t ptk_socket_unregister_events(ptk_loop_t *loop, ptk_socket_t *socket) {
    return ptk_unregister_socket_events(loop, socket);
}
