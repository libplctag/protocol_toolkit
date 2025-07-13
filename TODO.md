# Protocol Toolkit - TODO List

Based on analysis of the codebase and echo client/server sketches, here are the missing implementations and API fixes needed to make the sketches functional.

## 🔥 Critical - API Mismatches (Blocking sketches)

### 1. Fix `ptk_tcp_socket_connect()` API Mismatch
**Issue**: ✅ Header and implementation agree.


**Solution**: Update implementation to match header (create socket + connect in one call)

### 2. ✅ Address Functions - COMPLETED
**Status**: All address functions are now implemented and properly named
- ✅ `ptk_address_init()` - Initialize address from IP string and port
- ✅ `ptk_address_to_string()` - Convert address to string (proper signature)
- ✅ `ptk_address_get_port()` - Get port from address
- ✅ `ptk_address_equals()` - Compare two addresses
- ✅ `ptk_address_init_any()` - Initialize address for INADDR_ANY

**Note**: Functions use proper naming convention - `init` for initialization, `create` for allocation

## 🟡 Important - Missing Socket Functions

### 3. Missing Socket Management Functions
- `ptk_socket_abort()` - Abort ongoing socket operations
- `ptk_socket_wait()` - Wait for socket events with timeout
- `ptk_socket_signal()` - Signal a socket
### 4. ✅ Network Discovery Function Renamed
**Status**: Function renamed for consistency
- ✅ `ptk_socket_network_list()` - Renamed from `ptk_network_discover()` (was `ptk_socket_find_networks()` in header)

**Note**: Header and implementation now aligned with consistent naming

### 4. ✅ Buffer API Return Type Mismatch
**Issue**: Sketches expect `ptk_buf_get_len()` to return `size_t`, but implementation returns `buf_size_t`
- Current: `buf_size_t ptk_buf_get_len(const ptk_buf *buf);`
- Expected: Some sketches cast to `size_t`

**Decision needed**: Keep `buf_size_t` (16-bit) or change to `size_t`?

## 🟢 Low Priority - Network Discovery

### 5. Complete Network Discovery Implementation
- Verify `ptk_network_discover()` platform-specific implementation
- Implement `platform_discover_network()` function
- Test network interface enumeration

### 6. UDP Multicast Support
- Complete `ptk_udp_multicast_socket_create()` (currently stub)
- Add multicast join/leave functions

## 📁 Implementation Plan

### Phase 1: ✅ Address Functions - COMPLETED
```bash
# ✅ DONE: Address functions implemented in socket_common.c
# ✅ Functions properly named with init/create convention:
# - ptk_address_init() - Initialize existing memory
# - ptk_address_init_any() - Initialize for INADDR_ANY
# - ptk_address_to_string() - Create allocated string
# - ptk_address_get_port() - Get port value
# - ptk_address_equals() - Compare addresses
```

### Phase 2: Socket Connect Fix (Critical)
```bash
# Update src/lib/event_loop/socket_common.c
# Change ptk_tcp_socket_connect() to:
# 1. Create TCP socket
# 2. Set non-blocking
# 3. Connect to address
# 4. Return socket on success
```

### Phase 3: Missing Socket Functions (Important)
```bash
# Add to socket_common.c:
# - ptk_socket_abort()
# - ptk_socket_wait()
# - ptk_socket_signal()
# - ptk_socket_find_networks()
```

### Phase 4: Build System Updates
```bash
# Update CMakeLists.txt to include new source files
# Fix any missing dependencies
# Ensure sketches can build and run
```

## 🧪 Testing Strategy

### Unit Tests Needed
- [ ] Address function tests
- [ ] Socket connect/disconnect tests
- [ ] Buffer API compatibility tests
- [ ] Network discovery tests

### Integration Tests
- [ ] Echo client/server sketches should compile and run
- [ ] Multi-client server test
- [ ] Network interface discovery test
- [ ] UDP broadcast test

## 📋 File Changes Required

### New Files to Create
- `src/lib/ptk_sock_address.c` - Address manipulation functions
- `tests/test_ptk_sock_address.c` - Address function tests

### Files to Modify
- `src/lib/event_loop/socket_common.c` - ✅ Address functions added, fix connect API, add missing functions
- `src/lib/CMakeLists.txt` - Add new source files (if needed)
- `src/include/ptk_sock.h` - ✅ Address functions declared, fix any remaining typos

### Files to Review
- `echo_client_sketch.c` - Verify compatibility after fixes
- `echo_server_sketch.c` - Verify compatibility after fixes
- `src/lib/event_loop/socket_common.h` - Update function declarations

## 🎯 Success Criteria

### Minimal Success (MVP)
- [ ] Echo client sketch compiles without errors
- [ ] Echo server sketch compiles without errors
- [ ] Client can connect to server successfully
- [ ] Basic echo functionality works

### Full Success
- [ ] All socket API functions implemented
- [ ] Network discovery functional
- [ ] Multi-threaded server handles multiple clients
- [ ] UDP broadcast discovery works
- [ ] All tests pass

## 🐛 Known Issues to Address

1. **Error Code Mismatch**: Some functions return `PTK_WAIT_OK` vs `PTK_OK`
2. **Socket Structure**: Missing `event_loop` field assignment in some functions
3. **Memory Management**: Ensure all socket objects use proper destructors
4. **Thread Safety**: Verify threadlet integration works correctly
5. **Platform Support**: Test on both POSIX and Windows platforms

## 📝 Recent Progress

### ✅ Completed (Latest)
- **Address Functions**: All address manipulation functions implemented in `socket_common.c`
  - `ptk_address_init()` - Initialize address from IP/port (proper naming: init vs create)
  - `ptk_address_init_any()` - Initialize for INADDR_ANY
  - `ptk_address_to_string()` - Convert to string (allocates memory)
  - `ptk_address_get_port()` - Get port number
  - `ptk_address_equals()` - Compare addresses
- **Network Discovery**: `ptk_socket_network_list()` renamed and header aligned
- **Header/Implementation Sync**: Address function signatures now match between header and implementation

### 🎯 Next Priority: Fix `ptk_tcp_socket_connect()` API mismatch

## 📝 Notes

- Most of the hard work is already done - the event loop and threadlet systems are implemented
- Socket implementation exists but has API mismatches
- Address functions exist in attic but need to be moved to active codebase
- Buffer API is solid and well-designed
- The type-safe serialization system is impressive and complete

**Estimated effort**: 1-2 days for critical fixes, 1 week for full completion
