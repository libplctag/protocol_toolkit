# TODO: Functions Needed for Echo Server and Echo Client

This document lists all the functions that need to be implemented in order to create test programs for an echo server and an echo client using the APIs in `src/include`.

## General Requirements
- All functions must follow the code creation guidelines in `docs/code_creation_guidelines.md`.
- Only public API functions (exposed in `src/include`) should use the `ptk_` prefix.
- All memory allocation must use `ptk_alloc()` with appropriate destructors.
- Logging must use macros from `ptk_log.h`.

---

## Core API Functions (from `src/include`)

### Threading and Synchronization
 - `ptk_thread_create()` (done)
 - `ptk_thread_join()` (done)
 - `ptk_thread_destroy()` (done) -- TODO remove and make sure there is a destructor and ptk_alloc() is used.
 - `ptk_mutex_create()` (done)
 - `ptk_mutex_wait_lock()` (done)
 - `ptk_mutex_unlock()` (done)
 - `ptk_mutex_destroy()` (done) -- TODO, ptk_alloc() destructor
 - `ptk_cond_var_create()` (done)
 - `ptk_cond_var_wait()` (done)
 - `ptk_cond_var_signal()` (done)
 - `ptk_cond_var_destroy()` (done) -- TODO, ptk_alloc() destructor.

### Socket and Network
 - `ptk_tcp_client_connect()`
 - `ptk_tcp_client_recv()`
 - `ptk_tcp_client_send()`
 - `ptk_sock_close()`
 - `ptk_tcp_server_listen()`
 - `ptk_tcp_server_accept()`

### Event Loop

are these hallucinations?

 - `event_loop_create()` (stub)
 - `event_loop_add_fd()` (stub)
 - `event_loop_remove_fd()` (stub)
 - `event_loop_poll()` (stub)
 - `event_loop_wake()` (stub)
 - `event_loop_destroy()` (stub)

### Buffer Management
 - all done

### Error Handling
 - `ptk_err` codes must be used and documented in all public APIs.

---


## Echo Client Sketch

```c
int main() {
    ...

    ... set up ^C handler ...


    ptk_sock *client = ptk_tcp_client_connect(host, port);

    ...

    char buf[256] = {0};
    int counter = 1;

    while(!done) {
        int msg_len = snprintf(buf, sizeof(buf), "Message #%d is: Hello, Dolly!", counter++);

        ptk_buf *msg = ptk_buf_alloc_from_data(&buf[0], msg_len);

        ptk_err status = ptk_tcp_socket_send(client, msg);
        ...

        ptk_socket_wait(...);
    }
}

