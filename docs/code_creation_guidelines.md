Rules for creating functions:

- Functions must be spaced at least one blank line apart.
- Functions must have comments in Doxygen-compatible format that include all parameters and the return value.
- Where error handling is a bit non-standard (i.e. use of errno or  code like in ptk_err.h) document where and how the error can be retrieved.
- Creating or finding/search functions should return their creations. Use a different channel for error.
- Use the functions and definitions in ptk_log.h for all logging. The macros defined there will automatically include the surrounding function name and the line number in the file so do not put the
function name or line number in the log messages.
- Failures that are somewhat recoverable like running out of filedescriptors should log with warn().
- Failures that are catastrophic like malloc returning NULL should be logged with error().
- Function entry and exit should be logged at level info().
- Important steps within functions should be logged at debug().
- Code that would generate a lot of logging (i.e. big looks, callbacks every 10ms) should be logged at level trace().
- all allocation of memory should be done with ptk_alloc() and a destructor should be included if there is any attached data that needs to be freed or disposed of safely.  If you find yourself writing code that looks like this:
```c
     if(obj->field) { ptk_free(obj->field); }
     ptk_free(obj);
```
That means that the place where obj was created should have provided a destructor function.  There should only be one place that all the details of disposing of an object are known.
- if there is more than 10% of a file to change, rewrite it in a different file and mv it over.
- do not provide publically accessible destructor functions for your data.  Use the destructor callback with ptk_alloc().  All memory should be freed just with ptk_free().
- Only public functions have the ptk_ prefix.  Anything internal should not have that prefix.  Public functions are those that are in the public header files in src/include.
- Ownership of memory/data:
  - if a function returns a pointer, it was created with ptk_alloc() and you own it and need to free it with ptk_free().
  - if you pass a parameter to a function that is a pointer to a pointer, then the function owns the value and will NULL out your pointer to the data.  Obviously this does not help if you make multiple copies of the point. So don't do that.
- Code that is largely shared between all the POSIX platforms goes in src/lib/posix.  Source that is shared between BSD-like platforms goes in src/lib/bsd and includes all the BSD variants and macOS.   Source that is specific to Windows goes in src/lib/windows.
- Platform-specific event loop implementations:
  - Linux-specific code (epoll) goes in src/lib/linux
  - BSD/macOS-specific code (kqueue/kevent) goes in src/lib/bsd  
  - Windows-specific code (IOCP) goes in src/lib/windows
  - Cross-platform event loop interface goes in src/lib/posix
- Green thread (threadlet) implementation:
  - Use 64KiB stack size for threadlets
  - Threadlets are bound to OS threads and cannot migrate
  - Socket operations that would block should yield the threadlet and resume via event loop
  - Platform detection macros should be defined for conditional compilation 
- Shared memory/pointer usage (ptk_shared.h):
  - Only wrap memory that was allocated with ptk_alloc() - never wrap stack variables or static memory
  - Use ptk_shared_wrap() immediately after ptk_alloc() to avoid double-wrapping errors
  - Prefer the use_shared() macro over manual acquire/release for automatic lifecycle management
  - Always provide error handling with on_shared_fail when using use_shared() macro
  - Never store the raw pointer returned by ptk_shared_acquire() beyond the acquire/release pair
  - Handle reference count overflow gracefully - it indicates a design problem if you hit UINT32_MAX references
  - Shared memory is thread-safe but the data it points to may not be - add your own synchronization if needed
  - Example usage:
    ```c
    // Create and wrap memory
    my_struct_t *obj = ptk_alloc(sizeof(my_struct_t), my_struct_destructor);
    ptk_shared_handle_t handle = ptk_shared_wrap(obj);
    
    // Use with automatic lifecycle management
    use_shared(handle, my_struct_t *ptr) {
        ptr->field = 42;
        // ptr automatically released when block exits
    } on_shared_fail {
        error("Failed to acquire shared memory");
    }
    ```

