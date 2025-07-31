#include "protocol_toolkit.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/timerfd.h>
#include <sys/eventfd.h>
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

// Find available timer slot
static int ptk_find_timer_slot(ptk_ev_loop_t *loop) {
    for(int i = 0; i < PTK_MAX_TIMERS_PER_LOOP; i++) {
        if(!loop->impl.timers[i].in_use) { return i; }
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

// Register socket events with epoll
static ptk_error_t ptk_register_socket_events(ptk_ev_loop_t *loop, ptk_socket_t *socket, ptk_ev_source_t *read_source,
                                              ptk_ev_source_t *write_source) {
    if(!loop || !socket || loop->impl.epoll_fd == -1) { return PTK_ERR_INVALID_ARG; }

    struct epoll_event ev;
    ev.events = 0;
    ev.data.ptr = NULL;

    // Register read events if read_source provided
    if(read_source && !socket->impl.registered_read) {
        ev.events |= EPOLLIN;
        read_source->impl.type = PTK_ES_SOCKET;
        read_source->impl.fd = socket->socket_fd;
        read_source->impl.active = true;
        socket->impl.read_source = read_source;
        socket->impl.registered_read = true;
    }

    // Register write events if write_source provided
    if(write_source && !socket->impl.registered_write) {
        ev.events |= EPOLLOUT;
        write_source->impl.type = PTK_ES_SOCKET;
        write_source->impl.fd = socket->socket_fd;
        write_source->impl.active = true;
        socket->impl.write_source = write_source;
        socket->impl.registered_write = true;
    }

    // Use read_source as the data pointer, write events will be handled separately
    if(read_source) {
        ev.data.ptr = read_source;
    } else if(write_source) {
        ev.data.ptr = write_source;
    }

    if(ev.events != 0) {
        if(epoll_ctl(loop->impl.epoll_fd, EPOLL_CTL_ADD, socket->socket_fd, &ev) == -1) {
            return PTK_ERR_SOCKET_FAILURE;
        }
    }

    return PTK_SUCCESS;
}

// Unregister socket events from epoll
static ptk_error_t ptk_unregister_socket_events(ptk_ev_loop_t *loop, ptk_socket_t *socket) {
    if(!loop || !socket || loop->impl.epoll_fd == -1) { return PTK_ERR_INVALID_ARG; }

    if(socket->impl.registered_read || socket->impl.registered_write) {
        epoll_ctl(loop->impl.epoll_fd, EPOLL_CTL_DEL, socket->socket_fd, NULL);
        socket->impl.registered_read = false;
        socket->impl.registered_write = false;
        socket->impl.read_source = NULL;
        socket->impl.write_source = NULL;
    }

    return PTK_SUCCESS;
}

// =============================================================================
// EVENT LOOP IMPLEMENTATION
// =============================================================================

ptk_error_t ptk_loop_init(ptk_ev_loop_t *loop, ptk_sm_t *initial_sm) {
    if(!loop) { return PTK_ERR_INVALID_ARG; }

    // Initialize epoll
    loop->impl.epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if(loop->impl.epoll_fd == -1) { return PTK_ERR_SOCKET_FAILURE; }

    // Initialize state
    loop->current_sm = initial_sm;
    loop->platform_data = NULL;
    loop->impl.running = false;
    loop->impl.next_timer_id = 1;

    // Initialize timer slots
    for(int i = 0; i < PTK_MAX_TIMERS_PER_LOOP; i++) {
        loop->impl.timers[i].in_use = false;
        loop->impl.timers[i].source = NULL;
    }

    return PTK_SUCCESS;
}

void ptk_loop_run(ptk_ev_loop_t *loop) {
    if(!loop || loop->impl.epoll_fd == -1) { return; }

    loop->impl.running = true;

    while(loop->impl.running) {
        // Wait for events (1 second timeout)
        int nevents = epoll_wait(loop->impl.epoll_fd, loop->impl.events, PTK_MAX_EPOLL_EVENTS, 1000);

        ptk_time_ms current_time = ptk_get_time_ms();

        // Process epoll results
        for(int i = 0; i < nevents; i++) {
            struct epoll_event *ev = &loop->impl.events[i];
            ptk_ev_source_t *es = (ptk_ev_source_t *)ev->data.ptr;

            if(es && es->impl.owner_sm) {
                // Handle different event types
                if(es->impl.type == PTK_ES_TIMER) {
                    // Read from timerfd to acknowledge the timer
                    uint64_t expirations;
                    read(es->impl.fd, &expirations, sizeof(expirations));
                    
                    // Route timer event to the state machine
                    ptk_sm_handle_event(es->impl.owner_sm, es->event_id, es, current_time);
                } else if(es->impl.type == PTK_ES_SOCKET) {
                    // Route socket event to the state machine
                    ptk_sm_handle_event(es->impl.owner_sm, es->event_id, es, current_time);
                } else if(es->impl.type == PTK_ES_USER) {
                    // Read from eventfd to acknowledge the event
                    uint64_t value;
                    read(es->impl.fd, &value, sizeof(value));
                    
                    // Route user event to the state machine
                    ptk_sm_handle_event(es->impl.owner_sm, es->event_id, es, current_time);
                }
            }
        }
    }
}

void ptk_loop_stop(ptk_ev_loop_t *loop) {
    if(loop) { loop->impl.running = false; }
}

// =============================================================================
// EVENT SOURCE IMPLEMENTATION
// =============================================================================

ptk_error_t ptk_es_init_timer(ptk_ev_source_t *es, int event_id, ptk_time_ms interval_ms, bool periodic, void *user_data) {
    if(!es || interval_ms == 0) { return PTK_ERR_INVALID_ARG; }

    es->event_id = event_id;
    es->interval_ms = interval_ms;
    es->periodic = periodic;
    es->user_data = user_data;

    // Initialize Linux-specific data
    es->impl.type = PTK_ES_TIMER;
    es->impl.fd = -1;  // Will be set when attached to loop
    es->impl.active = false;

    return PTK_SUCCESS;
}

ptk_error_t ptk_es_init_user_event(ptk_ev_source_t *es, int event_id, void *user_data) {
    if(!es) { return PTK_ERR_INVALID_ARG; }

    es->event_id = event_id;
    es->interval_ms = 0;
    es->periodic = false;
    es->user_data = user_data;

    // Initialize Linux-specific data
    es->impl.type = PTK_ES_USER;
    es->impl.fd = -1;  // Will be set when attached to loop
    es->impl.active = false;

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
    sock->impl.nonblocking = true;
    sock->impl.registered_read = false;
    sock->impl.registered_write = false;
    sock->impl.read_source = NULL;
    sock->impl.write_source = NULL;

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
    sock->impl.nonblocking = true;
    sock->impl.registered_read = false;
    sock->impl.registered_write = false;
    sock->impl.read_source = NULL;
    sock->impl.write_source = NULL;

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
    sock->impl.nonblocking = true;
    sock->impl.registered_read = false;
    sock->impl.registered_write = false;
    sock->impl.read_source = NULL;
    sock->impl.write_source = NULL;

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
// STATE MACHINE IMPLEMENTATION
// =============================================================================

ptk_error_t ptk_sm_init(ptk_sm_t *sm, ptk_tt_t **tables, uint32_t max_tables,
                        ptk_ev_source_t **sources, uint32_t max_sources, ptk_ev_loop_t *loop, void *user_data) {
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

ptk_error_t ptk_sm_attach_event_source(ptk_sm_t *sm, ptk_ev_source_t *es) {
    if(!sm || !es || sm->source_count >= sm->max_sources) { return PTK_ERR_INVALID_ARG; }

    // Set the owner state machine
    es->impl.owner_sm = sm;

    // Handle timer event sources
    if(es->impl.type == PTK_ES_TIMER && sm->loop) {
        // Find available timer slot
        int slot = ptk_find_timer_slot(sm->loop);
        if(slot == -1) { return PTK_ERR_OUT_OF_BOUNDS; }

        // Create timerfd
        es->impl.fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
        if(es->impl.fd == -1) { return PTK_ERR_SOCKET_FAILURE; }

        // Configure timer
        struct itimerspec timer_spec;
        timer_spec.it_value.tv_sec = es->interval_ms / 1000;
        timer_spec.it_value.tv_nsec = (es->interval_ms % 1000) * 1000000;
        
        if(es->periodic) {
            timer_spec.it_interval = timer_spec.it_value;
        } else {
            timer_spec.it_interval.tv_sec = 0;
            timer_spec.it_interval.tv_nsec = 0;
        }

        if(timerfd_settime(es->impl.fd, 0, &timer_spec, NULL) == -1) {
            close(es->impl.fd);
            return PTK_ERR_SOCKET_FAILURE;
        }

        // Register with epoll
        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.ptr = es;
        if(epoll_ctl(sm->loop->impl.epoll_fd, EPOLL_CTL_ADD, es->impl.fd, &ev) == -1) {
            close(es->impl.fd);
            return PTK_ERR_SOCKET_FAILURE;
        }

        // Assign timer slot and activate
        sm->loop->impl.timers[slot].in_use = true;
        sm->loop->impl.timers[slot].source = es;
        es->impl.active = true;
    } else if(es->impl.type == PTK_ES_USER && sm->loop) {
        // Create eventfd for user events
        es->impl.fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        if(es->impl.fd == -1) { return PTK_ERR_SOCKET_FAILURE; }

        // Register with epoll
        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.ptr = es;
        if(epoll_ctl(sm->loop->impl.epoll_fd, EPOLL_CTL_ADD, es->impl.fd, &ev) == -1) {
            close(es->impl.fd);
            return PTK_ERR_SOCKET_FAILURE;
        }

        es->impl.active = true;
    }

    sm->sources[sm->source_count++] = es;
    return PTK_SUCCESS;
}

ptk_error_t ptk_sm_handle_event(ptk_sm_t *sm, int event_id, ptk_ev_source_t *es, ptk_time_ms now_ms) {
    if(!sm) { return PTK_ERR_INVALID_ARG; }

    // Search through transition tables for matching transition
    for(uint32_t i = 0; i < sm->table_count; i++) {
        ptk_tt_t *tt = sm->tables[i];
        for(uint32_t j = 0; j < tt->transition_count; j++) {
            ptk_tt_entry_t *trans = &tt->transitions[j];

            if(trans->initial_state == sm->current_state && trans->event_id == event_id) {
                // Execute action if present - CORRECTED signature to match header
                if(trans->action) { 
                    trans->action(sm, sm->current_state, tt, es, event_id, NULL, trans->next_state);
                }

                // Infrastructure handles state transition (not the action function)
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
// TRANSITION TABLE IMPLEMENTATION
// =============================================================================

ptk_error_t ptk_tt_init(ptk_tt_t *tt, ptk_tt_entry_t *transitions, uint32_t max_transitions) {
    if(!tt || !transitions) { return PTK_ERR_INVALID_ARG; }

    tt->transitions = transitions;
    tt->transition_count = 0;
    tt->max_transitions = max_transitions;

    return PTK_SUCCESS;
}

ptk_error_t ptk_tt_add_transition(ptk_tt_t *tt, int initial_state, int event_id, int next_state,
                                  ptk_sm_t *next_sm, ptk_action_func action) {
    if(!tt || tt->transition_count >= tt->max_transitions) { return PTK_ERR_INVALID_ARG; }

    ptk_tt_entry_t *trans = &tt->transitions[tt->transition_count];
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
    client_socket->impl.nonblocking = true;
    client_socket->impl.registered_read = false;
    client_socket->impl.registered_write = false;
    client_socket->impl.read_source = NULL;
    client_socket->impl.write_source = NULL;

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

ptk_error_t ptk_sm_attach_table(ptk_sm_t *sm, ptk_tt_t *tt) {
    if(!sm || !tt || sm->table_count >= sm->max_tables) { return PTK_ERR_INVALID_ARG; }

    sm->tables[sm->table_count++] = tt;
    return PTK_SUCCESS;
}

ptk_error_t ptk_sm_add_to_loop(ptk_ev_loop_t *loop, ptk_sm_t *sm) {
    if(!loop || !sm) { return PTK_ERR_INVALID_ARG; }

    loop->state_machines[loop->sm_count++] = sm;
    if(loop->sm_count > loop->max_sm_count) {
        /* error handling */
    }
    sm->loop = loop;
    return PTK_SUCCESS;
}

// Public socket event registration functions
ptk_error_t ptk_socket_register_events(ptk_ev_loop_t *loop, ptk_socket_t *socket, ptk_ev_source_t *read_source,
                                       ptk_ev_source_t *write_source) {
    return ptk_register_socket_events(loop, socket, read_source, write_source);
}

ptk_error_t ptk_socket_unregister_events(ptk_ev_loop_t *loop, ptk_socket_t *socket) {
    return ptk_unregister_socket_events(loop, socket);
}