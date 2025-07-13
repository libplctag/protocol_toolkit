#pragma once

// Include public socket API and common utilities
#include "ptk_sock.h"
#include "socket_common.h"

//=============================================================================
// UDP Socket Implementation
//=============================================================================

// All UDP socket functions are declared in ptk_sock.h:
// - ptk_udp_socket_create
// - ptk_udp_multicast_socket_create
// - ptk_udp_socket_send_to         (single packet, ptk_buf)
// - ptk_udp_socket_send_many_to    (multi-packet, ptk_udp_buf_entry_array_t)
// - ptk_udp_socket_recv_from       (single packet, returns ptk_buf)
// - ptk_udp_socket_recv_many_from  (multi-packet, returns ptk_udp_buf_entry_array_t)
// This header provides any internal UDP-specific utilities if needed.
