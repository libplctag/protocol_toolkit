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

**Note:** All public types (usually via typedef) must be named with a `_t` suffix (e.g., `ptk_buf_t`, `ptk_err_t`, `ptk_slice_bytes_t`).
All public types and functions must use the `ptk_` prefix. Internal/private types and functions must not use the `ptk_` prefix.
New public types must follow the `_t` naming convention. Implementation-specific or private structs should be forward declared in public headers and defined only in the corresponding implementation file.

**Implementation-specific header note:**
If a struct's size or layout is platform-dependent, the public header should forward-declare the struct and include the implementation-specific header (e.g., `#include "posix/ptk_x_impl.h"`) in the `.c` file to ensure correct sizing and layout. This allows the public API to remain portable and opaque while the implementation can use the correct platform-specific details.

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

- All memory allocation for temporary/thread-local data must use the scratch allocator API (`ptk_scratch_t*`, see `ptk_scratch.h`). Do not use `malloc()`/`free()` directly.
- Use `ptk_scratch_alloc()`, `ptk_scratch_alloc_aligned()`, `ptk_scratch_alloc_slice()`, or `ptk_slice_alloc()` for all buffer and array allocations.
- For persistent or shared data, use explicit user-provided buffers or platform-appropriate allocation APIs as required by the public API.
- Resource cleanup should be handled by the allocator or by explicit API calls; there is no destructor callback in the public API.
- Ownership rules:
  - If a function returns a pointer or slice, the caller owns it and must release it using the appropriate allocator or by resetting the scratch allocator.
  - If a function takes a pointer-to-pointer, it owns the value and will null out your pointer.

---




## 5. Logging and Debugging

- Use the logging macros in `ptk_log.h` for all logging. The available macros are:
  - `PTK_LOG(fmt, ...)` for general logging with file, function, and line number automatically included.
  - `PTK_LOG_SLICE(slice)` to print a byte slice as hex with context.
- There are no separate log levels; use `PTK_LOG` for all log messages.
- Do not include function names or line numbers in log messages; the macro adds these automatically.

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

- All code must be bounds-checked; avoid pointer arithmetic. Use the type-safe slice system (`ptk_slice_*_t` and helpers) for all buffer and array access.
- Use the provided slice macros and functions (see `ptk_slice.h`) for safe, type-checked access to arrays and buffers.
- Do not store raw pointers from shared memory or buffers beyond their valid lifetime.
- Internal implementation functions and data definitions within a `.c` file should always be `static`.
- Header files that declare public functions and data must use `extern` explicitly.
- Use static inline functions for type safety where possible. Macros are used for slice and serialization helpers where appropriate.

---