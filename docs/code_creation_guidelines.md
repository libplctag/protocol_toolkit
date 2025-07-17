# Protocol Toolkit Code Creation Guidelines

## Table of Contents

- [Protocol Toolkit Code Creation Guidelines](#protocol-toolkit-code-creation-guidelines)
  - [Table of Contents](#table-of-contents)
  - [1. File Naming and Structure](#1-file-naming-and-structure)
  - [2. Function Design and Documentation](#2-function-design-and-documentation)
  - [3. Error Handling](#3-error-handling)
  - [4. Memory Management](#4-memory-management)
  - [5. Logging and Debugging](#5-logging-and-debugging)
  - [6. Platform-Specific Code and Build](#6-platform-specific-code-and-build)
  - [7. Testing Guidelines](#7-testing-guidelines)
  - [8. General Coding Practices](#8-general-coding-practices)

---

## 1. File Naming and Structure
**Note:** All public types (usually via typedef) should be named with a `_t` suffix (e.g., `ptk_buf_t`, `ptk_err_t`).
Some existing types in `ptk_defs.h` do not follow this rule for historical reasons (e.g., `ptk_u8_t` should be `ptk_u8_t`). New public types must follow the `_t` naming convention.

- **Public API files** use the `ptk_` prefix (e.g., `ptk_sock.h`, `ptk_mem.c`).
- **Private/implementation files** must NOT use the `ptk_` prefix, and no platform-specific suffix (e.g., `os_thread.c`, `linux/address.c`).
- Platform-specific code is organized as:
  - `src/lib/posix/` for POSIX-shared code
  - `src/lib/bsd/` for BSD/macOS
  - `src/lib/linux/` for Linux
  - `src/lib/windows/` for Windows
- Never include stub or empty files unless explicitly needed for some build-related purpose.  Generally just do not do this.

**Note:** Only public structs and typedefs should be defined in public header files. Any implementation-specific or private struct should be forward declared in the public header (if needed) and defined only in the private implementation file (e.g., the corresponding `.c` file). This ensures encapsulation and prevents leaking internal details into the public API.


---

## 2. Function Design and Documentation

- Functions must be separated by at least one blank line.
- All functions must have Doxygen-compatible comments, including all parameters and the return value.
- Public functions are those declared in `src/include/ptk_*.h` and must use the `ptk_` prefix.
- Internal (non-public) functions must not use the `ptk_` prefix and should not appear in public headers.
- Internal functions and data in `.c` files should always be `static`.
- All public functions and data in headers must use `extern` explicitly.

---

## 3. Error Handling

- If error handling is non-standard (e.g., uses something other than `errno` or its equivalents or `ptk_err.h`), document how and where the error can be retrieved.
- Functions that create or find resources should return the resource, and use a separate channel for errors:
  - Pointer return: return `NULL` on error.
  - Unsigned int: return the maximum value on error.
  - Signed int: return the minimum value on error.
  - Float/double: return negative infinity on error.
  - Shared memory handle: return the value PTK_SHARED_INVALID_HANDLE on error.

---

## 4. Memory Management

- All thread-local memory allocation must use `ptk_local_alloc()` (not `malloc()`/`free()`).
- All data to be accessed by more than one thread must use `ptk_shared_alloc()` and use the `use_shared()` helper macro to avoid many errors with shared memory and locks.
- Always provide a destructor if any attached data needs to be freed or disposed of safely.
- If you find code like:
  ```c
  if(obj->field) { ptk_free(obj->field); }
  ptk_free(obj);
  ```
  then a destructor should have been provided at allocation.
- Do not provide public destructors; use the destructor callback with `ptk_local_alloc()` and `ptk_shared_alloc()`.
- All local memory should be freed with `ptk_local_free()`.
- All shared memory should be freed with `pkt_shared_free()`.
- Ownership rules:
  - If a function returns a pointer, the caller owns it and must free it with `ptk_local_free()`.
  - If a function takes a pointer-to-pointer, it owns the value and will null out your pointer.

**IMPORTANT: When using `use_shared()` or `on_shared_fail` blocks, you MUST NOT use `return` to exit the block. Only `break` is allowed for early exit.**

> **WARNING:**
>   Using `return` inside a `use_shared()` or `on_shared_fail` block will result in undefined behavior and may leak resources. The macro relies on structured control flow to ensure proper resource release. Always use `break` to exit these blocks early.

**Example (correct):**
```c
use_shared(handle, 100, my_struct_t *ptr) {
    if(ptr->field == 0) {
        break; // OK: releases resources properly
    }
    // ...
} on_shared_fail {
    // ...
}
```

**Example (incorrect, do NOT do this):**
```c
use_shared(handle, 100, my_struct_t *ptr) {
    if(ptr->field == 0) {
        return; // ERROR: do NOT use return here!
    }
    // ...
} on_shared_fail {
    // ...
}
```

This restriction is required for correct resource management!

---

## 5. Logging and Debugging

- Use the logging functions/macros in `ptk_log.h` for all logging.
- Do not include function names or line numbers in log messages (the macros do this automatically).
- Log levels:
  - `info()`: function entry/exit
  - `debug()`: important steps within functions
  - `warn()`: recoverable failures (e.g., out of file descriptors)
  - `error()`: catastrophic failures (e.g., allocation failure)
  - `trace()`: very frequent events (e.g., tight loops, frequent callbacks)

---

## 6. Platform-Specific Code and Build

- **Do not use C preprocessor dispatchers** (e.g., `#ifdef _WIN32 ... #elif defined(__linux__) ...`) in source files to select platform-specific implementations unless it is a small amount of code.  Use the platform directories and platform-specific code when more than 20% is platform specific.
- **Use CMake** to select and include the correct source files for the target platform.
  - CMake should append the appropriate files to the build using variables and platform checks (e.g., `if(WIN32) ... elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux") ...`).
  - This ensures only the correct implementation is compiled and linked for each platform, and avoids unnecessary code in the final binary.
- **Example:**
  In `src/lib/CMakeLists.txt`:
  ```cmake
  if(WIN32)
      list(APPEND PTK_PLATFORM_SOURCES windows/atomic_operations.c windows/os_thread_win.c)
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
      list(APPEND PTK_PLATFORM_SOURCES posix/atomic_operations.c posix/thread.c linux/address.c linux/sock.c)
  elseif(CMAKE_SYSTEM_NAME MATCHES "BSD|FreeBSD|OpenBSD|NetBSD")
      list(APPEND PTK_PLATFORM_SOURCES posix/atomic_operations.c posix/thread.c posix/network_list.c)
  endif()
  ```
- This approach keeps platform-specific code out of the build for other platforms, reduces binary size, and avoids accidental symbol conflicts.
- It also makes it clear in the build system which files are used for which platform.

---

## 7. Testing Guidelines

- Every public API function and data element must be tested.
- When testing a specific API file, do not use any function from that file except the one(s) under test.
- Test programs must return 0 on success and non-zero on failure.
- Tests that check memory/resource reclamation should be run under Valgrind (on supported platforms).
- Tests should use the APIs they are NOT testing.
- Unit tests are useful, but public API tests are more important and less fragile.
- Perform no pointer arithmetic in tests; use bounds-checked built-ins.

---

## 8. General Coding Practices

- All code must be bounds-checked; avoid pointer arithmetic.
- Use safe array/buffer accessors.
- Do not store raw pointers from shared memory beyond their acquire/release block.
- Handle reference count overflow gracefully in shared memory APIs.
- Shared memory is thread-safe, but the data it points to may not be—add your own synchronization if needed.
- Internal implementation functions and data definitions within a `.c` file should always be `static`.
- Header files that declare public functions and data must use `extern` explicitly.
- Static inline functions are typesafe.  Macros are not.  Do not use macros when it would be just as easy to add a static inline function.
- Use the safe array template.

---