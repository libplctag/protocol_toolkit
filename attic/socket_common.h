#pragma once

// Include public socket API
#include <ptk_sock.h>

// Include internal event loop dependencies
#include "event_registration.h"
#include "timeout_heap.h"
#include <ptk_threadlet.h>

//=============================================================================
// Common Socket Implementation Utilities
//=============================================================================

// Forward declaration of current threadlet (set by event loop)
extern threadlet_t *current_threadlet;

// Helper to set non-blocking mode
int set_nonblocking(int fd);

// Socket destructor called when ptk_free() is called on a socket
void ptk_socket_destructor(void *ptr);
