# General Implementation Notes

Overall goals:

- high portability to many OSes including the most popular RTOSes.
- No hidden memory allocation on embedded platforms and little to none on mainstream OSes.
- simplicity and safety over performance.  Do not use fancy data structures if a simple linear scan will find the data.
- Ideally the user would be responsible for all memory allocation.

## Handles

Handles are opaque int64_t values to the user. Internally they contain four components:

- **Type field (8 bits)**: Resource type (timer, socket, user event source, etc.)
- **Event Loop ID (8 bits)**: Which event loop owns this resource
- **Generation field (16 bits)**: Incremented each time a slot is reused
- **Handle ID field (32 bits)**: Unique incrementing identifier for each resource

Handle allocation strategy:

- **New Resource**: Increment global Handle ID counter, assign current generation for the slot
- **Reused Slot**: Increment generation counter for that slot to invalidate old handles
- **Event Loop Scoping**: Handle includes event loop ID for resource isolation
- **Dangling Handle Protection**: Old handles will have wrong generation and fail validation
- **Type Safety**: Handle type must match the resource array being searched

This approach provides safety against use-after-free while maintaining simplicity.
Linear scans match on exact int64_t value (type + event_loop_id + generation + handle_id).

## Handle-based Resources

All resource types use the same simple allocation strategy:

### Resource Arrays

User provides fixed-size arrays at initialization time. Each array contains
platform-specific structures that all begin with `ptk_resource_base_t`.

### Allocation Algorithm

```c
/* To create a new resource: */
1. Scan array for entry with handle.id = 0 (unused slot)
2. If found:
   a. Increment global Handle ID counter
   b. Increment generation counter for this slot
   c. Combine: type | (generation << 8) | (handle_id << 32)
   d. Store in slot and initialize platform-specific fields
3. Return the complete handle value

/* To find existing resource: */
1. Extract type from handle and verify it matches expected resource type
2. Scan appropriate array for entry with exact int64_t handle match
3. Return pointer to the entry or NULL if not found

/* To destroy resource: */
1. Find resource by exact handle match
2. Clean up platform-specific resources
3. Set handle.id = 0 to mark slot as free, do not set the generation or type to 0.
4. Generation will be incremented on next allocation to invalidate old handles
```

**Handle Format:**
```
Bits 63-32: Handle ID (unique incrementing counter)
Bits 31-16: Generation (incremented per slot reuse)
Bits 15-8:  Event Loop ID (which event loop owns this resource)
Bits 7-0:   Type (resource type identifier)
```

### Platform-Specific Implementation

Each platform defines its own `protocol_toolkit.h` with complete resource
structure definitions. No shared/generic header file.

## Protothreads

The user application is responsible for allocating the storage for these.  It is expected that the
application will wrap the existing ptk_pt_t structure with its own context:

```c
typedef struct {
    ptk_pt_t pt;

    int32_t app_connection_handle;
    uin16_t app_connection_sequence_num;
    uint64_t app_serial_num;
    const char *remote_addr;
    uint16_t remote_port;
    /* ... */
} app_pt_t;
```

Then the app will cast this when passing it to the ptk_protothread_init() function:

```
result_status = ptk_protothread_init((ptk_pt_t *)&app_context, pt_func);
```

## Event-Loop-Centric Resource Management

### Design Philosophy

Resources (timers, sockets, user events) are allocated per event loop rather than globally:

- **Event Loop Ownership**: Each event loop owns its resource pools
- **Independent Scaling**: Different event loops can have different resource limits
- **Clear Boundaries**: Resources are scoped to their owning event loop
- **Flexible Architecture**: Applications can optimize resource allocation per use case

### Initialization Process

```c
/* 1. Declare event loop slots */
PTK_DECLARE_EVENT_LOOP_SLOTS(my_event_loops, 3);

/* 2. Declare resource pools for different event loops */
PTK_DECLARE_EVENT_LOOP_RESOURCES(ui_resources, 2, 4, 1);      /* Minimal UI loop */
PTK_DECLARE_EVENT_LOOP_RESOURCES(net_resources, 8, 64, 16);   /* Heavy network loop */

/* 3. No global PTK initialization needed */

/* 4. Create event loops by providing slots and resources directly */
ptk_handle_t ui_loop = ptk_event_loop_create(my_event_loops, 3, &ui_resources);
ptk_handle_t net_loop = ptk_event_loop_create(my_event_loops, 3, &net_resources);

/* 5. Create resources within specific event loops */
ptk_handle_t ui_timer = ptk_timer_create(ui_loop);
ptk_handle_t net_socket = ptk_socket_create(net_loop);
```

**Process Details:**

1. **Event Loop Slot Allocation**: Linear scan of provided event loop slots for handle = 0
2. **Resource Assignment**: Assign resource arrays to the found slot
3. **Handle Creation**: Event loop handle references the slot index within the provided array
4. **Resource Handles**: Include event loop ID (slot index) for lookup within the same slot array

### API Changes

Resource creation functions now require an event loop parameter:

```c
/* Old global approach */
ptk_handle_t ptk_timer_create(void);
ptk_handle_t ptk_socket_create(void);

/* New event-loop-scoped approach */
ptk_handle_t ptk_timer_create(ptk_handle_t event_loop);
ptk_handle_t ptk_socket_create(ptk_handle_t event_loop);
```

### Benefits

- **Resource Isolation**: Network event loop can have many sockets, UI loop can have few
- **Memory Efficiency**: Allocate resources where they're actually needed
- **Debugging**: Easier to track which event loop owns which resources
- **Threading**: Natural boundary for thread-local resource pools
- **Scaling**: Can resize resources for specific event loops independently

The application is responsible for calling ptk_event_loop_run() in a loop on its own.  It can use a single thread or multiple threads to run multiple event loops.  Each event loop runs in a single thread.

The number of event sources the event loop can handle depends on the
internal implementation so the application must catch error returns that indicate that the event loop is out of resources.

### Raising Events

Events can be raised from any thread in the application.  So the the
function ptk_raise_event() must be thread safe.  User events are the only events that can be raised across threads.

### Setting Event Handlers

It is not supported to try to set an event handler for an event source in a different thread.  This should be checked in debug mode
but not in release mode.

### Application/User Events

Applications can create any number (up to the limits for the platform) of user event sources.  Those event sources can have
up to some fixed number of event slots.  TBD: Need a defintions for that somewhere.   The events that can be
raised on those event sources include all the builtin events and
any that the application defines.

### General Event Source Implementation

Use the simplest code that could work.  Keep in mind that the
event values could be varied and not necessarily continuous.   I.e.
a user event source could take event values 1, 16, and 2049.   So
allocating an array with the event values being the index is not viable.  Keeping a smaller array with the individual event values
and matching them via a linear search is fine.   Simplicity over
performance.

### Event Handling

An event can be raised even if there is not a handler for it.  When
a handler is set for a raised event, the event loop must run it as
soon as possible.  The event handler _only_ runs in the context of the thread running the event loop, not the context of the thread
raising the event.

#### Platform Specific Handling

Some events will be raised all the time if enabled.  For instance
it would be likely that the writeable event would be raised all the
time if enabled.   That would cause the underlying event loop implementation to constantly return without waiting.  So the implementation will need to disable the event if nothing is waiting
for it.  This contradicts the above point.  So event-by-event we will need to see which ones should be able to be enabled before use.

Perhaps we need to have two flags: enabled and raised?  Once raised, an event could be disabled until a handler is called.

### Embedded Systems Limitations

Embedded systems like NuttX, Zephyr and FreeRTOS have very strict
limitations on memory use.  For those platforms the total number of
allowed event sources will be a fixed number defined by the
application when the library is compiled.

## Platform-Specific Header Strategy

### No Generic Header File

Instead of a shared `protocol_toolkit.h` with platform-specific includes, each
platform provides its own complete `protocol_toolkit.h` file:

```
src/include/
├── linux/
│   └── protocol_toolkit.h      (Linux-specific complete header)
├── windows/
│   └── protocol_toolkit.h      (Windows-specific complete header)
├── freertos/
│   └── protocol_toolkit.h      (FreeRTOS-specific complete header)
└── macos/
    └── protocol_toolkit.h      (macOS-specific complete header)
```

### CMake Integration

CMake selects the appropriate platform directory and copies the header
to the build include directory:

```cmake
# Platform detection
if(UNIX AND NOT APPLE)
    set(PTK_PLATFORM "linux")
elseif(APPLE)
    set(PTK_PLATFORM "macos")
elseif(WIN32)
    set(PTK_PLATFORM "windows")
endif()

# Copy platform-specific header
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/include/${PTK_PLATFORM}/protocol_toolkit.h"
    "${CMAKE_CURRENT_BINARY_DIR}/include/protocol_toolkit.h"
    COPYONLY
)
```

### User Application Example

```c
#include "protocol_toolkit.h"

/* Application declares event loop slots */
PTK_DECLARE_EVENT_LOOP_SLOTS(app_event_loops, 2);

/* Application declares resource pools */
PTK_DECLARE_EVENT_LOOP_RESOURCES(main_resources, 8, 16, 4);
PTK_DECLARE_EVENT_LOOP_RESOURCES(network_resources, 4, 32, 2);

int main() {
    /* Create event loops */
    ptk_handle_t main_loop = ptk_event_loop_create(app_event_loops, 2, &main_resources);
    ptk_handle_t network_loop = ptk_event_loop_create(app_event_loops, 2, &network_resources);

    /* Resources are scoped to their event loop */
    ptk_handle_t timer = ptk_timer_create(main_loop);
    ptk_handle_t socket = ptk_socket_create(network_loop);

    /* Run event loops */
    while (1) {
        ptk_event_loop_run(main_loop);
        ptk_event_loop_run(network_loop);
    }
}
```
```

### Benefits

- **No Global State**: All resources are application-managed
- **Zero Runtime Allocation**: All memory pre-allocated at compile time
- **Event Loop Isolation**: Resources cannot accidentally cross event loop boundaries
- **Type Safety**: Handle validation prevents type mismatches
- **Generational Safety**: Stale handle detection through generation counters

### Handle ID Uniqueness Strategy

**Challenge**: Without global state, ensuring unique handle IDs across event loops requires careful consideration.

**Options:**

1. **Global Atomic Counter** (Minimal Global State):
   ```c
   static _Atomic uint16_t ptk_next_handle_id = 1;
   ```
   - Pros: True global uniqueness, minimal global state
   - Cons: Breaks pure no-global-state principle

2. **Application-Provided Counter**:
   ```c
   ptk_handle_t ptk_timer_create(ptk_handle_t event_loop, uint16_t *next_id);
   ```
   - Pros: No global state, application controls uniqueness
   - Cons: More complex API, application must manage counter

3. **Event-Loop-Scoped IDs** (Current Choice):
   - Handle IDs unique within each event loop only
   - Full handle (type|event_loop_id|generation|handle_id) is globally unique
   - Pros: Simplest implementation, no global state
   - Cons: Handle ID field alone not globally unique

**Selected Approach**: Event-Loop-Scoped IDs

The combination of (event_loop_id + handle_id) provides global uniqueness while maintaining zero global state. This aligns with the event-loop-centric design where resources are naturally scoped to their owning event loop.

### Eliminating Hidden Global State

**Challenge**: Even with application-provided event loop slots, we still had hidden global state:

```c
/* Hidden global state - not acceptable */
static _Atomic uint8_t ptk_next_event_loop_id = 1;
```

**Solution**: Application-Managed Event Loop IDs

Event loop IDs are simply the index within the application-provided slots array:

```c
/* No global counter needed */
ptk_handle_t ptk_event_loop_create(ptk_event_loop_slot_t *slots,
                                   size_t num_slots,
                                   ptk_event_loop_resources_t *resources) {
    /* Linear scan for unused slot */
    for (size_t i = 0; i < num_slots; i++) {
        if (slots[i].handle == 0) {
            /* Event loop ID = slot index */
            uint8_t event_loop_id = (uint8_t)i;
            slots[i].handle = PTK_MAKE_HANDLE(PTK_TYPE_EVENT_LOOP, event_loop_id, 1, i);
            slots[i].resources = resources;
            slots[i].last_error = PTK_ERR_OK;
            return slots[i].handle;
        }
    }
    return PTK_ERR_OUT_OF_MEMORY;  /* No available slots */
}
```

### Event-Loop-Scoped Error Handling

**Problem**: Thread-local storage isn't portable to all embedded platforms.

**Solution**: Store error state in the event loop structure, access via resource's stored event loop handle.

**API Changes**:

```c
/* Error handling now takes any resource handle to identify event loop */
ptk_err_t ptk_get_last_error(ptk_handle_t any_resource_handle);
void ptk_set_last_error(ptk_handle_t any_resource_handle, ptk_err_t error);

/* Resource creation stores owning event loop handle */
ptk_handle_t ptk_timer_create(ptk_handle_t event_loop) {
    /* Find event loop */
    ptk_event_loop_slot_t *loop_slot = find_event_loop(event_loop);
    if (!loop_slot) {
        /* Can't set error - no valid event loop! Return negative error code */
        return PTK_ERR_INVALID_HANDLE;
    }

    /* Find free timer slot */
    for (size_t i = 0; i < loop_slot->resources->num_timers; i++) {
        ptk_timer_internal_t *timer = &loop_slot->resources->timers[i];
        if (timer->base.handle == 0) {
            /* Store both timer handle and owning event loop handle */
            timer->base.handle = PTK_MAKE_HANDLE(PTK_TYPE_TIMER,
                                                PTK_HANDLE_EVENT_LOOP_ID(event_loop),
                                                ++timer->generation_counter,
                                                i);
            timer->base.event_loop = event_loop;  /* Store for error reporting */
            return timer->base.handle;
        }
    }

    /* Set error and return */
    loop_slot->last_error = PTK_ERR_OUT_OF_MEMORY;
    return PTK_ERR_OUT_OF_MEMORY;
}

/* Operations can report errors via stored event loop handle */
ptk_err_t ptk_timer_start(ptk_handle_t timer_handle, uint64_t interval_ms) {
    ptk_timer_internal_t *timer = find_timer(timer_handle);
    if (!timer) {
        return PTK_ERR_INVALID_HANDLE;
    }

    /* Use stored event loop handle for error reporting */
    if (interval_ms == 0) {
        ptk_set_last_error(timer->base.event_loop, PTK_ERR_INVALID_ARGUMENT);
        return PTK_ERR_INVALID_ARGUMENT;
    }

    /* ... perform operation ... */
    return PTK_ERR_OK;
}
```

**Benefits**:
- **Zero Global State**: No atomic counters, no thread-local storage
- **Portable**: Works on all embedded platforms
- **Event Loop Isolation**: Errors are scoped to the owning event loop
- **Resource-Centric**: Any resource handle can be used to access error state
- **Simple Implementation**: Linear scan for event loop slots, index becomes ID

- **Zero complexity** in header management
- **Full platform optimization** - each platform can define optimal structures
- **No preprocessor complexity** - no #ifdef blocks in shared headers
- **Complete type visibility** - user can allocate exact platform structures
- **Simplified build system** - just copy the right header file

## General Coding

Place implementation code in src/lib/XYZ where XYZ is something like "linux", "windows", "bsd", "nuttx", "freertos" etc.

Do not try to share code between implementations at this stage.  Just implement everything even though there might be signicant duplication.
Refactoring will take place after the library is fully functional.

The most common event support on the platform should be used.  I.e.
epoll on Linux/Android, kevent on macOS/BSD/iOS, IOCP on Windows.
Fall back to select for other POSIX platforms.  The embedded platform
offer differing native network features.  If needed use the lowest
common denominator for the platform.  I.e. do not require lwIP if
the implementation does not really need to have it.

- GCC, Clang and MSVC are the primary supported compilers.  Where
  they are freely available, care should be taken to support
  special embedded compilers.
- All code should conform to the C99 standard.
- Comments should use /* */ only. No C++-like // comments.
- All functions and data definitions should have Doxygen-compliant comments, including "@brief".

## General Compilation

- The library will only be built as a static, non-shared library.
  This is to allow the embedded platforms to remove as much code as
  possible when linking.
- LTO should be enabled when building the non-debug version.
- Maximum settings for small binary size should be enabled for the non-debug version.
- All logging should be completely left out if NDEBUG is defined.
- CMake is to be used for all building and packaging.
- Packages should be built for:
  -  Debian,
  -  RPM,
  -  Alpine,
  -  macOS (home brew?),
  -  Windows (MSI?),
  -  vcpkg?
  -  conan package manager?
  -  What others are popular?
-  The CMake namespaced library name should be ptk::ptk.
-  If the library is used as a Git submodule, the examples and tests
   should not be built or run.  Only code necessary to build an application should be compiled.
- the two support CMake build types should be Debug and MinSizeRel. The latter should define NDEBUG and use all necessary compilation
  and link flags to minimize the size of the binary.

## Open Questions

- Should the event loop run in its own thread automatically?
  - Pro:
    - Applications would be a bit simpler.
    - Application code would be more cross-platform.
  - Con:
    - This hides potential allocation from the app builder.
    - The lowest common denominator may be quite low and that could be a problem for some applications if we decide on one way to implement this and the application designer wants another.
  - Could this just be a boolean or a compile time flag that controls
    whether or not a thread is automatically used?   That poses API
    confusion possibly.  Maybe two api calls? One where the code takes
    all responsibility and another where it is delegated to the library?
 - Should the protothreads be handles too and use the handle user data
   pointer to point to application context?  That would remove one
   more possible barrier to wrapping the library API in another
   programming language like Python or Java or C#.  C++ can use it
   as is.
 - What other challenges are there for wrapping with non-C-based languages?  Buffers are clearly an issue.
 - Are other languages important?
 - Is use of a 64-bit handle too large for smaller embedded systems?
   Which ones do not support 64-bit integers?  Should there be
   a mapping for handles for 32-bit integers?
 - Would use of an arena allocator be helpful to reduce complexity
   within implementation code?  If its buffer was provided by
   the user, it would count as memory allocation being controlled by
   the user.

## Memory Management Strategy

### Core Principles (CLARIFIED)

- **NO runtime allocation during application execution** - this is the highest priority
- **Allocation only at startup** is acceptable if needed
- **Fixed-size buffers only** - no dynamic buffer resizing
- **User-provided memory** for all application-visible structures
- **Pre-allocated arrays** with free-list management for long-lived resources
- **Arena allocator** ONLY for ephemeral operations with clear reset points

### Resource Allocation Strategy

**Long-Lived Resources (sockets, timers, event sources):**

- Fixed-size arrays provided by user at initialization
- Linear scan to find free entries (handle value = 0) or existing entries (exact match)
- Each resource has a common base struct as the first field
- No free-lists, no bitmaps, no complex data structures
- Simple and deterministic performance

**Base Resource Structure:**

All platform-specific resource structs include a common base:

```c
typedef struct ptk_resource_base {
    ptk_handle_t handle;           /* Complete handle: type|generation|handle_id, 0 = unused */
    ptk_handle_t event_loop;       /* Handle of owning event loop (for error reporting) */
} ptk_resource_base_t;

/* Handle manipulation macros */
#define PTK_HANDLE_TYPE(h)           ((uint8_t)((h) & 0xFF))
#define PTK_HANDLE_EVENT_LOOP_ID(h)  ((uint8_t)(((h) >> 8) & 0xFF))
#define PTK_HANDLE_GENERATION(h)     ((uint16_t)(((h) >> 16) & 0xFFFF))
#define PTK_HANDLE_ID(h)             ((uint32_t)((h) >> 32))
#define PTK_MAKE_HANDLE(type, loop_id, gen, id) \
    ((ptk_handle_t)(type) | ((ptk_handle_t)(loop_id) << 8) | \
     ((ptk_handle_t)(gen) << 16) | ((ptk_handle_t)(id) << 32))

/* Example platform-specific timer struct */
typedef struct ptk_timer_internal {
    ptk_resource_base_t base;   /* Must be first field - contains handle + event_loop */
    /* Platform-specific timer fields follow... */
    int timerfd;                /* Linux-specific */
    uint64_t interval_ms;
    bool is_repeating;
    uint32_t generation_counter; /* Per-slot generation counter */
} ptk_timer_internal_t;

/* Resource pools structure - per event loop */
typedef struct ptk_event_loop_resources {
    ptk_timer_internal_t *timers;
    size_t num_timers;

    ptk_socket_internal_t *sockets;
    size_t num_sockets;

    ptk_user_event_source_internal_t *user_event_sources;
    size_t num_user_event_sources;
} ptk_event_loop_resources_t;

/* Event loop slot - provided by application */
typedef struct ptk_event_loop_slot {
    ptk_handle_t handle;                      /* 0 = unused event loop slot */
    ptk_event_loop_resources_t *resources;    /* Assigned when event loop created */
    ptk_err_t last_error;                     /* Event-loop-scoped error state */
    /* Platform-specific event loop data follows... */
} ptk_event_loop_slot_t;

/* No global config needed - just declare slots as needed */

/* Declare event loop slots */
#define PTK_DECLARE_EVENT_LOOP_SLOTS(name, max_loops) \
    static ptk_event_loop_slot_t name[max_loops]

/* Improved declaration macro for event loop resources */
#define PTK_DECLARE_EVENT_LOOP_RESOURCES(name, timers, sockets, user_events) \
    static ptk_timer_internal_t name##_timers[timers]; \
    static ptk_socket_internal_t name##_sockets[sockets]; \
    static ptk_user_event_source_internal_t name##_user_events[user_events]; \
    static ptk_event_loop_resources_t name = { \
        .timers = name##_timers, .num_timers = timers, \
        .sockets = name##_sockets, .num_sockets = sockets, \
        .user_event_sources = name##_user_events, .num_user_event_sources = user_events \
    }
```

**Ephemeral Operations (packet processing, string parsing):**

- Arena allocator with user-provided memory
- Reset after each complete operation
- Used for temporary buffers, intermediate data structures
- Clear lifetime boundaries prevent memory leaks

### Arena Allocator Usage

An arena allocator will be used internally ONLY for ephemeral allocations with clear lifetimes:

- Arena memory provided by user at initialization
- Used ONLY for temporary operations: network data encoding/decoding, string parsing, etc.
- Arena reset after each complete operation (e.g., after processing a network packet)
- NOT used for long-lived resources (sockets, timers, event sources) which need individual deallocation
- All arena allocation fails gracefully when memory exhausted

### Long-Lived Resource Management

For sockets, timers, and user event sources:

- **Event Loop Scoped**: Resources belong to a specific event loop
- **Per-Loop Arrays**: Each event loop has its own pre-allocated resource arrays
- **Handle Format**: Handles encode event loop ID for validation and lookup
- **Linear Scan**: Within the appropriate event loop's arrays
- **Generation Safety**: Per-slot generation counters prevent dangling handle reuse
- **Type Safety**: Handle type must match the resource array being accessed

**Resource Lookup Process:**

1. **Extract Event Loop ID**: From handle to identify which event loop owns the resource
2. **Validate Event Loop**: Ensure the event loop handle is valid
3. **Type Check**: Verify handle type matches the expected resource type
4. **Linear Scan**: Search the appropriate array in the owning event loop
5. **Exact Match**: Compare complete int64_t handle value

**Safety Features:**

- **Event Loop Isolation**: Resources from one event loop cannot accidentally affect another
- **Dangling Handle Protection**: Old handles have wrong generation and fail validation
- **Type Safety**: Handle type must match the resource array being accessed
- **Scope Validation**: Handles include event loop information for additional validation

### Runtime Resource Pool Resizing (Optional)

Since resources reference each other via handles (not pointers), event loop resources
can be resized at runtime:

```c
/**
 * @brief Replace event loop resources with new larger/smaller pools
 * @param event_loop_handle Handle to the event loop
 * @param new_resources New resource pools for this event loop
 * @return Status code
 */
ptk_err_t ptk_event_loop_resize_resources(ptk_handle_t event_loop_handle,
                                          ptk_event_loop_resources_t *new_resources);
```

**Resizing Process:**

1. **Copy Active Resources**: Copy all non-zero handle entries to new pools
2. **Update Event Loop State**: Replace resource pointers and sizes atomically
3. **Handle Validation**: Existing handles remain valid (same int64_t values)
4. **Thread Safety**: Ensure no resource operations on this event loop during resize

**Benefits:**

- **Per-Event-Loop Scaling**: Each event loop can have different resource limits
- **Independent Scaling**: Resize one event loop without affecting others
- **Handle Stability**: Existing handles remain valid across resize
- **Application Control**: Different event loops can have different resource strategies

**Example Usage:**

```c
/* Start with small pools for main event loop */
PTK_DECLARE_EVENT_LOOP_RESOURCES(initial_main, 4, 8, 2);
ptk_handle_t main_loop = ptk_event_loop_create(&initial_main);

/* Later, need more resources for main event loop */
PTK_DECLARE_EVENT_LOOP_RESOURCES(expanded_main, 16, 32, 8);
ptk_event_loop_resize_resources(main_loop, &expanded_main);
```

## Error Handling Strategy (CLARIFIED)

### Direct Return Codes

Functions that perform operations return ptk_err_t directly:

- Socket read/write operations
- Timer start/stop
- Event handler registration

### Last Error Pattern

Functions that create or find resources use ptk_set_last_err()/ptk_get_last_err():

- Handle creation functions (return handle or negative error)
- Resource lookup operations
- Initialization functions

### No Real-Time Guarantees

- Industrial network protocols inherently have network delays and retries
- "Real-time friendly" (bounded execution) is sufficient
- Priority is correctness and safety for long-running embedded systems

## Event Loop Ownership Strategy

### Application-Controlled (Primary)

- Application calls ptk_event_loop_run() directly
- Application manages threading/tasking
- Maximum control and predictability

### Library-Managed (Optional)

- Internal thread/task creation for convenience
- Compile-time or runtime configuration option
- Must not hide allocation from application
