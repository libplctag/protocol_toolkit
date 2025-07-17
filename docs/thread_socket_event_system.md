# Thread and Socket Event System Design: Signal-Driven Blocking Operation Aborts

## Problem Statement

Protocol Toolkit requires that threads can be signaled (e.g., abort, terminate, wake) and that any ongoing blocking operation—whether a thread wait or a socket operation (connect, send, receive, accept)—must be immediately aborted and return control to the caller. This is essential for responsive shutdown, error handling, and thread management.

Currently, the APIs in `ptk_os_thread.h` and `ptk_sock.h` do not guarantee that signals (especially aborts) will interrupt blocking socket operations. Furthermore, sockets may be created in one thread and handed off to another, requiring dynamic event queue management.

## Key Requirements

- **Signal-Driven Abort:** Any blocking operation (thread wait, socket connect/send/recv/accept) must be interruptible by thread signals, especially aborts.
- **Thread Event Queues:** Each thread must have an internal event system (e.g., epoll, kqueue, IOCP) to manage sockets and signals.
- **Socket Ownership Transfer:** Sockets can be moved between threads; event queue membership must be updated safely and atomically.
- **User Simplicity:** The API should be simple, safe, and easy for end users to use correctly.

## Thread Argument Passing API

The thread argument API is explicit and type-safe. The user provides a type value for each argument, allowing the thread function to identify and retrieve arguments by type and index. Only five functions are used to add arguments to a thread before it starts:

### Argument Addition Functions

```c
// Add a pointer argument (user provides type value)
ptk_err_t ptk_thread_add_ptr_arg(ptk_thread_handle_t thread, int user_type, void **ptr);

// Add an unsigned integer argument
ptk_err_t ptk_thread_add_uint_arg(ptk_thread_handle_t thread, int user_type, uint64_t val);

// Add a signed integer argument
ptk_err_t ptk_thread_add_int_arg(ptk_thread_handle_t thread, int user_type, int64_t val);

// Add a floating-point argument (double precision)
ptk_err_t ptk_thread_add_float_arg(ptk_thread_handle_t thread, int user_type, double val);

// Add a shared handle argument
ptk_err_t ptk_thread_add_handle_arg(ptk_thread_handle_t thread, int user_type, ptk_shared_handle_t *handle);
```

- For pointer and handle arguments, the function takes the address of the pointer/handle and nulls it after adding.  This is an ownership transfer.  While not perfect it will help in almost all cases where a parent thread or process creates data and then passes it to a thread for processing.
- The `user_type` field is an integer value chosen by the user to identify the argument type (e.g., enum, tag, or constant).
- All arguments are added before the thread is started.

### Thread Start and Run Function

```c
// Create a thread handle (does not start execution)
ptk_thread_handle_t ptk_thread_create(void);

// Set the thread's run function
ptk_err_t ptk_thread_set_run_function(ptk_thread_handle_t thread, ptk_thread_func func);

// Start the thread (executes the run function with the provided arguments)
ptk_err_t ptk_thread_start(ptk_thread_handle_t thread);
```

### Argument Retrieval Functions (only valid in the running thread)

```c
// Get the number of arguments passed to the thread
size_t ptk_thread_get_arg_count(void);

// Get the user-provided type value for the argument at index
int ptk_thread_get_arg_type(size_t index);

// Get a pointer argument by index
void *ptk_thread_get_ptr_arg(size_t index);

// Get an unsigned integer argument by index
uint64_t ptk_thread_get_uint_arg(size_t index);

// Get a signed integer argument by index
int64_t ptk_thread_get_int_arg(size_t index);

// Get a floating-point argument by index
double ptk_thread_get_float_arg(size_t index);

// Get a shared handle argument by index
ptk_shared_handle_t ptk_thread_get_handle_arg(size_t index);
```

- All retrieval functions only work in the context of the running thread (i.e., inside the thread's run function).
- Arguments are indexed in the order they were added.
- The user_type value allows the thread to identify argument types and handle them appropriately.

### Example Usage

```c
// User code to set up and start a thread
ptk_thread_handle_t thread = ptk_thread_create();
int my_type = MY_TYPE_ARRAY;
void *my_array = ...;
ptk_thread_add_ptr_arg(thread, my_type, &my_array); // Function nulls my_array after adding
ptk_thread_add_int_arg(thread, MY_TYPE_COUNT, 42);
ptk_thread_set_run_function(thread, my_thread_func);
ptk_thread_start(thread);

// In the thread function
void my_thread_func(ptk_shared_handle_t self) {
    size_t argc = ptk_thread_get_arg_count();
    for (size_t i = 0; i < argc; ++i) {
        int type = ptk_thread_get_arg_type(i);
        if (type == MY_TYPE_ARRAY) {
            void *arr = ptk_thread_get_ptr_arg(i);
            // ...
        } else if (type == MY_TYPE_COUNT) {
            int64_t count = ptk_thread_get_int_arg(i);
            // ...
        }
    }
}
```

## Adopted Approach: Implicit Ownership via API Calls

Socket operations (e.g., accept, connect) automatically transfer socket ownership to the calling thread. Sockets do not often switch threads, so performance impact is minimal. The implementation of the API should make this transfer seamless and safe for users.

### Requirements for Existing Thread and Socket Functions

#### Thread Functions (`ptk_os_thread.h`)
- **ptk_thread_wait**: Must be interruptible by signals, especially aborts. Should return promptly when signaled.
- **ptk_thread_signal**: Must deliver signals reliably to the target thread and wake up any blocking operations.
- **ptk_thread_self**: Should allow querying the current thread for event queue management.
- **Signal Management Functions**: Must allow querying, clearing, and checking signals for correct event handling.

#### Socket Functions (`ptk_sock.h`)
- **ptk_tcp_connect / ptk_tcp_accept / ptk_tcp_socket_send / ptk_tcp_socket_recv**: All blocking socket operations must be interruptible by thread signals. If a thread receives an abort signal, the operation must be aborted and return control.
- **ptk_network_list_interfaces**: Should not require event queue management, but must be safe to call from any thread.
- **Socket Ownership**: When a socket is used in a thread (e.g., after accept), it must be atomically moved to that thread's event queue. The API must ensure the socket is removed from the previous thread's queue.
- **ptk_socket_close**: Must clean up the socket from the event queue and release resources safely.

## Implementation Details and Answers

1. **Thread Event System Initialization:**
   - On thread creation, each thread gets its own event system. This is internal and hidden from the user.
2. **Argument Passing and Ownership Transfer:**
   - Thread arguments are used to move data, with automatic nulling of the arguments passed. This covers most cases, though not perfect.
3. **Signal Handling in Blocking Operations:**
   - Blocking operations internally handle the event system returning with signal values and check those values to determine if an abort or wake signal was received.
4. **Socket Ownership Querying:**
   - No explicit query is needed; socket ownership is transferred to the thread. The thread itself is a shared handle and can be passed around as needed.
5. **Socket Sharing Between Threads:**
   - Sockets are not shared between threads; ownership is always transferred.
6. **Error Codes for Aborted Operations:**
   - As previously stated, blocking operations aborted by a signal return the appropriate error code (e.g., `PTK_ERR_SIGNAL`).
7. **Documentation and Examples:**
   - All requirements, edge cases, and usage patterns are documented, with examples provided for argument validation and thread configuration.

## Summary

With implicit socket ownership transfer, the event system must:
- Atomically move sockets to the calling thread's event queue on use.
- Ensure all blocking operations are interruptible by thread signals.
- Provide clear, simple APIs for users, with robust internal management.
- Document requirements and edge cases for thread and socket functions.
