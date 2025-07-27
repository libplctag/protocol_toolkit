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

- **`PT_BEGIN(pt)`**
  - Marks the start of a ptk_pt function.
  - Must be the first statement in the ptk_pt.

- **`PT_END(pt)`**
  - Marks the end of a ptk_pt function.
  - Indicates that the ptk_pt is finished.

- **`PT_WAIT_UNTIL(pt, condition)`**
  - Suspends the ptk_pt until the specified condition is true.

- **`PT_WAIT_UNTIL_WITH_EVENT(pt, src, evt, condition)`**
  - Suspends the ptk_pt, registers it with the event queue for the specified event, and resumes when the condition is true.

- **`suspend_pt(pt, src, evt)`**
  - A low-level macro that suspends the ptk_pt on the specified event queue.

---

### **2. Event Source API**

#### **Event Queue Structure**
```c
typedef struct ptk_event_queue {
    ptk_pt_t* head;                     // Head of the ptk_pt queue
    ptk_pt_t* tail;                     // Tail of the ptk_pt queue
    pthread_mutex_t lock;                    // Mutex for thread-safe access
} ptk_event_queue_t;
```

#### **Event Source Structure**
```c
typedef struct ptk_event_source {
    ptk_event_queue_t events[MAX_EVENTS];        // Array of event queues for specific events
} ptk_event_source_t;
```

#### **Functions**

- **`void enqueue_protothread(ptk_event_queue_t* queue, ptk_pt_t* pt)`**
  - Adds a ptk_pt to the specified event queue (thread-safe).

- **`ptk_pt_t* dequeue_protothread(ptk_event_queue_t* queue)`**
  - Removes and returns the first ptk_pt from the specified event queue (thread-safe).

- **`void raise_event(ptk_event_source_t* src, int event_type)`**
  - Marks an event as raised and schedules all protothreads waiting for the event.

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
    suspend_pt(pt, src, evt)
```

#### **`receive_data(pt, sock)`**
Waits for incoming data on a socket and reads the data after the wait ends.
```c
#define receive_data(pt, sock, buffer, len) \
    do { \
        suspend_pt(pt, sock, EVENT_READABLE); \
        read_socket(sock, buffer, len); \
    } while (0)
```

#### **`send_data(pt, sock, buffer, len)`**
Waits for the socket to become writable and sends the data after the wait ends.
```c
#define send_data(pt, sock, buffer, len) \
    do { \
        suspend_pt(pt, sock, EVENT_WRITABLE); \
        write_socket(sock, buffer, len); \
    } while (0)
```

#### **`sleep_for_ms(pt, timer, delay_ms)`**
Waits for the specified delay using a pre-initialized timer.
```c
#define sleep_for_ms(pt, timer, delay_ms) \
    do { \
        (timer)->expiry_time = current_time() + (delay_ms); \
        suspend_pt(pt, timer, TIMER_EVENT); \
    } while (0)
```

---

## **Example Usage**

### **Request-Response Workflow**
```c
void request_handler_protothread(ptk_pt_t* pt, void* arg) {
    PT_BEGIN(pt);

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

    PT_END(pt);
}
```

---

## **Appendix: Implementation Examples**

### **1. Event Queue Management**
```c
void enqueue_protothread(ptk_event_queue_t* queue, ptk_pt_t* pt) {
    pthread_mutex_lock(&queue->lock);

    if (queue->tail) {
        queue->tail->next = pt;
    } else {
        queue->head = pt;
    }
    queue->tail = pt;
    pt->next = NULL;

    pthread_mutex_unlock(&queue->lock);
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

End of Document.