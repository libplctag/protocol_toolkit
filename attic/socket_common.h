#pragma once
#include "ptk_sock.h"
#include "event_registration.h"
#include "timeout_heap.h"
#include "threadlet.h"

ptk_err ptk_tcp_socket_recv(ptk_sock *sock, ptk_buf *data, ptk_duration_ms timeout_ms);
ptk_err ptk_tcp_socket_send(ptk_sock *sock, ptk_buf *data, ptk_duration_ms timeout_ms);
ptk_sock *ptk_tcp_socket_connect(const ptk_address_t *remote_addr, ptk_duration_ms timeout_ms);
ptk_sock *ptk_tcp_socket_listen(const ptk_address_t *local_addr, int backlog);