#pragma once
#include "ptk_sock.h"
#include "event_registration.h"
#include "timeout_heap.h"
#include "threadlet.h"

ptk_wait_status ptk_tcp_socket_read(ptk_sock *sock, ptk_buf *data, ptk_duration_ms timeout_ms);
ptk_wait_status ptk_tcp_socket_write(ptk_sock *sock, ptk_buf *data, ptk_duration_ms timeout_ms);
ptk_wait_status ptk_tcp_socket_connect(ptk_sock *sock, const ptk_address_t *remote_addr, ptk_duration_ms timeout_ms);
// ...other socket APIs...
