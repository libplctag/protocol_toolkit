# Detailed Explanation of libuv and State Machine Functions

This document describes the key functions from `libuv` and the state machine design. Each function includes a detailed explanation of its parameters, behavior, and callback signatures, sufficient to reimplement these components in C using the C11 standard. Additionally, the proposed structs for the state machine implementation are included.

---

## **libuv Functions**

### **1. Initialization and Tear Down**

#### **a. uv_loop_init**
Initializes a new event loop.

- **Parameters:**
  - `uv_loop_t* loop`: A pointer to an uninitialized `uv_loop_t` structure.

- **Behavior:**
  Allocates and initializes the internal structures for the event loop. This function must be called before any operations involving the loop.

---

#### **b. uv_loop_close**
Closes an event loop.

- **Parameters:**
  - `uv_loop_t* loop`: A pointer to the initialized `uv_loop_t` structure.

- **Behavior:**
  Releases all resources associated with the event loop. The loop must not have any active handles or requests when this function is called (all handles must be closed first).

---

#### **c. uv_run**
Runs the event loop.

- **Parameters:**
  - `uv_loop_t* loop`: The event loop to run.
  - `uv_run_mode mode`: The mode to run the loop in. Can be one of:
    - `UV_RUN_DEFAULT`: Runs the loop until no active handles or requests remain.
    - `UV_RUN_ONCE`: Polls for I/O events once and returns.
    - `UV_RUN_NOWAIT`: Polls for I/O events without blocking and returns immediately.

- **Behavior:**
  Starts the event loop, processing events until the specified condition is met. This function drives the entire program's execution by repeatedly invoking callbacks for ready events.

---

#### **d. uv_default_loop**
Retrieves the default event loop.

- **Parameters:**
  None.

- **Returns:**
  A pointer to the default `uv_loop_t` structure.

- **Behavior:**
  Returns a pointer to the globally shared default event loop. This loop is allocated and initialized automatically and can be used instead of creating a custom loop.

---

### **2. TCP Functions**

#### **a. uv_tcp_init**
Initializes a TCP handle.

- **Parameters:**
  - `uv_loop_t* loop`: A pointer to the event loop where the handle will be registered.
  - `uv_tcp_t* handle`: A pointer to a user-allocated TCP handle structure.

- **Behavior:**
  Initializes a TCP handle and binds it to the provided event loop. This must be called before using the handle.

---

#### **b. uv_tcp_connect**
Connects a TCP handle to a remote server.

- **Parameters:**
  - `uv_connect_t* req`: A request object for the connection operation.
  - `uv_tcp_t* handle`: The TCP handle to use for the connection.
  - `const struct sockaddr* addr`: The address of the remote server.
  - `uv_connect_cb cb`: A callback invoked when the connection is established or fails.

- **Callback Signature (`uv_connect_cb`):**
  ```c
  void connect_cb(uv_connect_t* req, int status);
  ```
  - `uv_connect_t* req`: The connection request.
  - `int status`: The status of the connection. A value of `0` indicates success, while a negative value indicates an error.

- **Behavior:**
  Initiates a connection to a remote server. The `cb` is called upon success or failure.

---

#### **c. uv_tcp_bind**
Binds a TCP handle to a local address and port.

- **Parameters:**
  - `uv_tcp_t* handle`: The TCP handle to bind.
  - `const struct sockaddr* addr`: The local address to bind to.
  - `unsigned int flags`: Optional flags for the binding (e.g., IPv6 support).

- **Behavior:**
  Binds the TCP handle to the specified local address and port, enabling it to accept incoming connections.

---

#### **d. uv_listen**
Puts a TCP handle into listening mode.

- **Parameters:**
  - `uv_stream_t* stream`: A stream handle (typically `uv_tcp_t`) to listen for connections.
  - `int backlog`: The maximum number of pending connections.
  - `uv_connection_cb cb`: A callback invoked when a new connection is accepted.

- **Callback Signature (`uv_connection_cb`):**
  ```c
  void connection_cb(uv_stream_t* server, int status);
  ```
  - `uv_stream_t* server`: The server handle.
  - `int status`: The status of the new connection. A value of `0` means a new connection is ready to be accepted, while a negative value indicates an error.

- **Behavior:**
  Starts listening for incoming connections. When a connection is ready, the `cb` is invoked.

---

#### **e. uv_accept**
Accepts an incoming connection.

- **Parameters:**
  - `uv_stream_t* server`: The server stream handle.
  - `uv_stream_t* client`: A client stream handle to use for the accepted connection.

- **Behavior:**
  Accepts a pending connection on the server handle and assigns it to the client handle.

---

#### **f. uv_read_start**
Begins reading data from a stream.

- **Parameters:**
  - `uv_stream_t* stream`: The stream handle to read from.
  - `uv_alloc_cb alloc_cb`: A callback to allocate memory for incoming data.
  - `uv_read_cb read_cb`: A callback invoked when data is read or an error occurs.

- **Callback Signatures:**
  - **`uv_alloc_cb`:**
    ```c
    void alloc_cb(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);
    ```
    - `uv_handle_t* handle`: The handle for which the buffer is being allocated.
    - `size_t suggested_size`: The suggested size for the buffer.
    - `uv_buf_t* buf`: A structure to store the allocated buffer.

  - **`uv_read_cb`:**
    ```c
    void read_cb(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf);
    ```
    - `uv_stream_t* stream`: The stream handle.
    - `ssize_t nread`: The number of bytes read (`< 0` indicates an error).
    - `const uv_buf_t* buf`: The buffer containing the data.

- **Behavior:**
  Monitors the stream for readable data and invokes the `alloc_cb` and `read_cb` as needed.

---

### **3. Timers**

#### **a. uv_timer_init**
Initializes a timer handle.

- **Parameters:**
  - `uv_loop_t* loop`: The event loop where the handle will be registered.
  - `uv_timer_t* handle`: A pointer to a user-allocated timer handle.

- **Behavior:**
  Prepares the timer handle for use.

---

#### **b. uv_timer_start**
Starts a timer.

- **Parameters:**
  - `uv_timer_t* handle`: The timer handle to start.
  - `uv_timer_cb cb`: A callback invoked when the timer expires.
  - `uint64_t timeout`: Time in milliseconds before the first expiration.
  - `uint64_t repeat`: Time in milliseconds for subsequent expirations (set to `0` for one-shot timers).

- **Callback Signature (`uv_timer_cb`):**
  ```c
  void timer_cb(uv_timer_t* handle);
  ```
  - `uv_timer_t* handle`: The timer handle.

- **Behavior:**
  Starts the timer. The `cb` is invoked when the timer expires.

---

#### **c. uv_timer_stop**
Stops a running timer.

- **Parameters:**
  - `uv_timer_t* handle`: The timer handle to stop.

- **Behavior:**
  Stops the timer if it is running.

---

## **State Machine Functions**

### **1. State Machine Initialization**

#### **a. state_machine_init**
Initializes a state machine.

- **Parameters:**
  - `StateMachine* sm`: Pointer to the state machine instance.
  - `State initial_state`: The initial state of the state machine.
  - `TransitionTable* transitions`: Pointer to the transition table.
  - `void* context`: Optional context for the state machine.

- **Behavior:**
  Sets up the state machine with its initial state, transition table, and context.

---

### **2. Event Processing**

#### **a. state_machine_process_event**
Processes an event in the state machine.

- **Parameters:**
  - `StateMachine* sm`: Pointer to the state machine instance.
  - `Event event`: The event to process.

- **Behavior:**
  Looks up the appropriate transition for the current state and event, executes the associated action, and updates the state.

---

### **3. State and Context Access**

#### **a. state_machine_get_state**
Retrieves the current state of the state machine.

- **Parameters:**
  - `StateMachine* sm`: Pointer to the state machine instance.

- **Behavior:**
  Returns the current state.

---

#### **b. state_machine_get_context**
Retrieves the context associated with the state machine.

- **Parameters:**
  - `StateMachine* sm`: Pointer to the state machine instance.

- **Behavior:**
  Returns the context pointer.

---

### **4. Transition Management**

#### **a. find_transition**
Finds a transition for a given state and event.

- **Parameters:**
  - `TransitionTable* table`: Pointer to the transition table.
  - `State current_state`: The current state.
  - `Event event`: The event to process.

- **Behavior:**
  Searches the transition table for a matching state-event pair and returns the associated transition.

---

#### **b. execute_transition**
Executes a transition in the state machine.

- **Parameters:**
  - `StateMachine* sm`: Pointer to the state machine instance.
  - `Transition* transition`: The transition to execute.

- **Behavior:**
  Executes the action associated with the transition and updates the state machine's state.

---

### **5. Proposed Structs for the State Machine**

#### **a. State Machine Definition**
```c
typedef enum {
    STATE_IDLE,
    STATE_WAITING,
    STATE_PROCESSING,
    STATE_DONE
} State;

typedef enum {
    EVENT_START,
    EVENT_DATA_RECEIVED,
    EVENT_TIMEOUT,
    EVENT_COMPLETE
} Event;

typedef struct {
    State next_state;
    void (*action)(void* context); // Function pointer for the action to execute
} Transition;

typedef struct {
    State current_state;           // Current state of the state machine
    void* context;                 // Pointer to any user-defined context
    Transition* transitions;       // Pointer to the transition table
    size_t num_transitions;        // Number of transitions in the table
} StateMachine;
```

#### **b. Transition Table Struct**
```c
typedef struct {
    Transition* transitions;       // Array of state-event transitions
    size_t num_transitions;        // Number of transitions in the table
} TransitionTable;
```

---

This document now includes the proposed structs for the state machine implementation, along with all previously included details about `libuv` and state machine functions.
