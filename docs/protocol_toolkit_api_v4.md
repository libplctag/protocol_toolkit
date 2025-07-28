# **Protothread-Based Event-Driven System**

## **Overview**

This system provides a lightweight, event-driven framework for managing cooperative multitasking using protothreads. Protothreads allow you to write sequential-looking code while leveraging event-driven mechanisms underneath. The system is designed for embedded environments with constrained resources, such as network protocol stacks.

---

## **Definitions**

### **1. Protothread**
A ptk_pt (PT) is a lightweight, stackless coroutine that allows cooperative multitasking. It executes sequential logic but yields control explicitly using macros. Only one ptk_pt runs at a time, and it can suspend itself to wait for events.

### **2. Event**
An event is a signal that something has occurred, such as:
- A socket becoming readable/writable.
- A timer expiring.
- A hardware interrupt.

### **3. Event Source**
An event source is a producer of events. Examples include:
- Sockets (for network I/O events).
- Timers (for timeout or periodic events).
- Queues (for task or message notifications).

Each event source maintains one or more event queues, which hold protothreads waiting for specific events.

### **4. Event Queue**
An event queue is a linked list or circular buffer that stores protothreads waiting for a specific event from an event source. When the event occurs, protothreads are dequeued and executed by the event loop.

### **5. Event Loop**
The event loop is the core of the system. It:
1. Monitors all event sources.
2. Processes events by dequeuing protothreads waiting for those events.
3. Executes protothreads until they suspend themselves or finish execution.

---

## **System Requirements**

### **1. Lightweight Design**
- Use minimal memory (stackless protothreads, small data structures).
- Ensure efficient CPU utilization by avoiding unnecessary polling or context switching.

### **2. Event-Driven Architecture**
- Allow protothreads to suspend themselves and wait for specific events.
- Resume protothreads only when their events occur.

### **3. Thread-Safe Event Raising**
- Ensure event sources can raise events safely from other threads or interrupt service routines (ISRs).
- Use appropriate synchronization (e.g., mutexes or atomic operations) for event queues.

### **4. Dynamic Event Switching**
- Support protothreads dynamically switching between different events (e.g., waiting for a request, then waiting for a response).

### **5. Scalability**
- Handle multiple protothreads, event sources, and events efficiently without excessive overhead.

### **6. Debugging**
- Provide utilities for logging and debugging ptk_pt states, event queues, and event handling.

---

## **APIs**

### **1. Protothread API**

#### **Protothread Structure**
```c
typedef struct ptk_pt {
    void (*function)(struct ptk_pt* self, void* arg); // Function pointer for the ptk_pt
    void* context;                                         // User-defined context (e.g., socket or app state)
    struct ptk_pt* next;                              // Next ptk_pt in the event queue
} ptk_pt_t;
```

#### **Macros**

- **`PTK_PT_BEGIN(pt)`**
  - Marks the start of a protothread function.
  - Must be the first statement in the ptk_pt.

- **`PTK_PT_END(pt)`**
  - Marks the end of a protothread function.
  - Indicates that the protothread is finished.


---

### **2. Event Source API**

#### **Event Queue Structure**
```c
typedef struct ptk_event_queue {
    ptk_pt_t* waiting_pt;                // Single protothread waiting for the event
    pthread_mutex_t lock;               // Mutex for thread-safe access
} ptk_event_queue_t;
```

#### **Event Source Structure**
```c
typedef struct ptk_event_source {
    ptk_pt_t* events[MAX_EVENTS];        // Array of pointers to protothreads waiting for specific events
    pthread_mutex_t lock;               // Mutex for thread-safe access
} ptk_event_source_t;
```

#### **Functions**

- **`void set_event_handler(ptk_event_source_t* src, int event_type, ptk_pt_t* handler)`**
  - Sets the handler for a specific event in the event source.

- **`void remove_event_handler(ptk_event_source_t* src, int event_type)`**
  - Removes the handler for a specific event in the event source.

---

### **3. Timer API**

#### **Timer Structure**
```c
typedef struct ptk_timer {
    uint64_t expiry_time;                    // Absolute time when the timer expires
    int active;                              // Whether the timer is active
} ptk_timer_t;
```

#### **Functions**

- **`void timer_init(ptk_timer_t* timer)`**
  - Initializes the timer object.

- **`void sleep_for_ms(ptk_pt_t* pt, ptk_timer_t* timer, uint64_t delay_ms)`**
  - Suspends the ptk_pt until the specified delay (in milliseconds) has elapsed.

---

### **4. Event Loop API**

#### **Functions**

- **`void process_event(ptk_event_source_t* src, int event_type)`**
  - Processes all protothreads waiting for the specified event.

- **`void run_event_loop(void)`**
  - Continuously monitors event sources and processes events as they occur.

---

### **5. Convenience Macros**

#### **`wait_for_event(pt, src, evt)`**
Waits for a specific event on a specific event source.
```c
#define wait_for_event(pt, src, evt) \
    do { \
        pthread_mutex_lock(&(src)->events[evt].lock); \
        if ((src)->events[evt].tail) { \
            (src)->events[evt].tail->next = (pt); \
        } else { \
            (src)->events[evt].head = (pt); \
        } \
        (src)->events[evt].tail = (pt); \
        (pt)->next = NULL; \
        pthread_mutex_unlock(&(src)->events[evt].lock); \
    } while (0)
```

#### **`receive_data(pt, sock)`**
Waits for incoming data on a socket and reads the data after the wait ends.
```c
#define receive_data(pt, sock, buffer, len) \
    do { \
        wait_for_event(pt, sock, EVENT_READABLE); \
        read_socket(sock, buffer, len); \
    } while (0)
```

#### **`send_data(pt, sock, buffer, len)`**
Waits for the socket to become writable and sends the data after the wait ends.
```c
#define send_data(pt, sock, buffer, len) \
    do { \
        wait_for_event(pt, sock, EVENT_WRITABLE); \
        write_socket(sock, buffer, len); \
    } while (0)
```

#### **`sleep_for_ms(pt, timer, delay_ms)`**
Waits for the specified delay using a pre-initialized timer.
```c
#define sleep_for_ms(pt, timer, delay_ms) \
    do { \
        (timer)->expiry_time = current_time() + (delay_ms); \
        wait_for_event(pt, timer, TIMER_EVENT); \
    } while (0)
```

---

## **Example Usage**

### **Request-Response Workflow**
```c
void request_handler_protothread(ptk_pt_t* pt, void* arg) {
    PTK_PT_BEGIN(pt);

    request_queue_t* queue = (request_queue_t*)arg;
    char response_buffer[256];
    char request_buffer[256];

    while (1) {
        // Wait for a trigger event
        wait_for_event(pt, queue->event_source, REQUEST_TRIGGER);

        while (!is_queue_empty(queue)) {
            request_t* req = dequeue_request(queue);

            // Send the request
            send_data(pt, req->socket, req->data, req->length);

            // Wait for the response
            receive_data(pt, req->socket, response_buffer, sizeof(response_buffer));

            process_response(response_buffer);
            free_request(req);
        }
    }

    PTK_PT_END(pt);
}
```

---

## **Appendix: Implementation Examples**

### **1. Event Queue Management**
```c
void set_event_handler(ptk_event_source_t* src, int event_type, ptk_pt_t* handler) {
    pthread_mutex_lock(&src->lock);

    src->events[event_type] = handler; // Assign the handler to the event slot

    pthread_mutex_unlock(&src->lock);
}

void remove_event_handler(ptk_event_source_t* src, int event_type) {
    pthread_mutex_lock(&src->lock);

    src->events[event_type] = NULL; // Clear the event slot

    pthread_mutex_unlock(&src->lock);
}
```

### **2. Timer Example**
```c
void timer_init(ptk_timer_t* timer) {
    timer->expiry_time = 0;
    timer->active = 0;
}
```

### **3. Socket Read and Write**
```c
int read_socket(socket_t* sock, char* buffer, size_t len) {
    // Example implementation of reading from a socket
    return recv(sock->fd, buffer, len, 0);
}

int write_socket(socket_t* sock, const char* buffer, size_t len) {
    // Example implementation of writing to a socket
    return send(sock->fd, buffer, len, 0);
}
```

---

## **Event Raising API**

### **1. Raise Event Function**

```c
// Marks an event as raised and schedules the handler for the event
void raise_event(ptk_event_source_t* src, int event_type) {
    pthread_mutex_lock(&src->lock);

    ptk_pt_t* handler = src->events[event_type]; // Retrieve the handler for the event
    if (handler) {
        handler->function(handler, handler->context); // Execute the handler
    }

    pthread_mutex_unlock(&src->lock);
}
```

---

### **Updated API Documentation**

#### Event Loop API

- **`ptk_event_loop_create()`**: Creates a new event loop.

- **`ptk_event_loop_run(ptk_handle_t loop)`**: Runs the event loop. The application is responsible for calling this in a loop.

#### Timer API

- **`ptk_timer_start(uint64_t interval_ms, bool is_repeating)`**: Starts a timer with the specified interval and mode (one-shot or repeating).

- **`ptk_timer_stop(ptk_handle_t timer_handle)`**: Stops a running timer.

- **`ptk_timer_is_running(ptk_handle_t timer_handle)`**: Checks if a timer is currently running.

#### Error Handling

- **`ptk_get_last_err()`**: Retrieves the last error code.

- **`ptk_set_last_err(int err)`**: Sets the last error code.

#### Updated Macros

- **`ptk_wait_for_event(pt, src, evt)`**: Waits for a specific event on a source and registers the protothread.

- **`ptk_receive_data(pt, sock, buffer, len)`**: Waits for incoming data on a socket and reads it.

- **`ptk_send_data(pt, sock, buffer, len)`**: Waits for a socket to become writable and sends data.

- **`ptk_sleep_ms(pt, timer, delay_ms)`**: Suspends the protothread for a specified delay using a timer.
