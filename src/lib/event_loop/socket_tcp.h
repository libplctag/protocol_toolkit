#pragma once

// Include public socket API and common utilities
#include "ptk_sock.h"
#include "socket_common.h"

//=============================================================================
// TCP Socket Implementation
//=============================================================================

// All TCP socket functions are declared in ptk_sock.h:
// - ptk_tcp_socket_listen
// - ptk_tcp_socket_accept
// - ptk_tcp_socket_connect
// - ptk_tcp_socket_create
// - ptk_tcp_socket_send
// - ptk_tcp_socket_recv
// This header provides any internal TCP-specific utilities if needed.
