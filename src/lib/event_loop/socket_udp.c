#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#ifdef __linux__
#include <linux/socket.h> // For struct mmsghdr, sendmmsg, recvmmsg
#endif
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ptk_err.h>
#include <ptk_alloc.h>
#include <ptk_log.h>
#include <ptk_buf.h>
#include <ptk_array.h>
#include "socket_udp.h"
#include "socket_internal.h"
#include "event_loop.h"
#include "event_registration.h"
#include "timeout_heap.h"
#include "threadlet_integration.h"
#include "event_loop.h"
#include "event_registration.h"
#include "timeout_heap.h"
#include "threadlet_integration.h"

//=============================================================================
// UDP Socket Functions
//=============================================================================

/**
 * Create a UDP socket.
 * 
 * This function creates a UDP socket and binds it to the specified local address if provided.
 * If the broadcast flag is true, it enables the SO_BROADCAST option on the socket.
 * If local_addr is NULL, the socket is created without binding (for sending only).
 * @param local_addr Address to bind to (NULL for client-only).
 * @param broadcast If true, enables SO_BROADCAST option on the socket.
 * @return Valid UDP socket on success, NULL on failure (ptk_get_err() set).
 *         On failure, ptk_get_err() will be set to an appropriate error code.
 */
ptk_sock *ptk_udp_socket_create(const ptk_address_t *local_addr, bool broadcast) {
    debug("ptk_udp_socket_create: entry");
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        warn("socket() failed: %s", strerror(errno));
        return NULL;
    }
    set_nonblocking(fd);
    if (broadcast) {
        int opt = 1;
        if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt)) < 0) {
            warn("setsockopt(SO_BROADCAST) failed: %s", strerror(errno));
            close(fd);
            return NULL;
        }
        trace("SO_BROADCAST enabled on UDP socket");
    }
    if (local_addr) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = local_addr->ip;
        addr.sin_port = htons(local_addr->port);
        if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            warn("bind() failed: %s", strerror(errno));
            close(fd);
            return NULL;
        }
    }
    ptk_sock *sock = ptk_alloc(sizeof(ptk_sock), ptk_socket_destructor);
    if (!sock) {
        warn("ptk_alloc for ptk_sock failed");
        close(fd);
        return NULL;
    }
    sock->fd = fd;
    sock->type = PTK_SOCK_UDP;
    debug("ptk_udp_socket_create: exit");
    return sock;
}

/**
 * Create a UDP multicast socket (stub).
 *
 * This function should create a UDP socket, bind to INADDR_ANY and the group port,
 * and join the specified multicast group. Additional options (TTL, loopback, interface)
 * may be set as needed.
 * @param group_addr Multicast group address to join.
 * @param port Port to bind to.
 * @return Valid UDP socket on success, NULL on failure (ptk_get_err() set).
 */
ptk_sock *ptk_udp_multicast_socket_create(const char *group_addr, uint16_t port) {
    debug("ptk_udp_multicast_socket_create: entry");
    // TODO: Implement UDP multicast socket creation
    debug("ptk_udp_multicast_socket_create: exit");
    return NULL;
}

//=============================================================================
// Single-Packet UDP Functions (New API)
//=============================================================================

/**
 * Send a single UDP packet to a specific address (blocking).
 * This function sends a single buffer as a UDP packet.
 * If the socket is not writable, it registers for write events and yields the current threadlet.
 * @param sock UDP socket (must be valid and of type PTK_SOCK_UDP).
 * @param data Buffer containing data to send (must be valid and have data).
 * @param dest_addr Destination address to send to (cannot be NULL).
 * @param broadcast If true, allows sending to broadcast addresses.
 * @param timeout_ms Timeout for the send operation (in milliseconds). If 0, wait indefinitely.
 * @return PTK_OK on success, PTK_ERR_TIMEOUT on timeout, PTK_ERR_NETWORK_ERROR on error (ptk_get_err() set).
 */
ptk_err ptk_udp_socket_send_to(ptk_sock *sock, ptk_buf *data, const ptk_address_t *dest_addr, bool broadcast, ptk_duration_ms timeout_ms) {
    info("ptk_udp_socket_send_to: entry");
    
    if (!sock || sock->type != PTK_SOCK_UDP || !data || !dest_addr) {
        warn("Invalid arguments to ptk_udp_socket_send_to");
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return PTK_ERR_INVALID_PARAM;
    }
    
    size_t data_len = data->end - data->start;
    if (data_len == 0) {
        debug("Empty buffer, nothing to send");
        info("ptk_udp_socket_send_to: exit");
        return PTK_OK;
    }
    
    int fd = sock->fd;
    
    // Prepare destination address
    struct sockaddr_in dest_sockaddr;
    memset(&dest_sockaddr, 0, sizeof(dest_sockaddr));
    dest_sockaddr.sin_family = AF_INET;
    dest_sockaddr.sin_addr.s_addr = dest_addr->ip;
    dest_sockaddr.sin_port = htons(dest_addr->port);
    
    debug("Sending %zu bytes to UDP socket", data_len);
    ssize_t bytes_sent = sendto(fd, data->data + data->start, data_len, MSG_DONTWAIT,
                               (struct sockaddr *)&dest_sockaddr, sizeof(dest_sockaddr));
    
    if (bytes_sent >= 0) {
        debug("sendto() sent %zd bytes", bytes_sent);
        data->start += bytes_sent; // Update buffer start position
        info("ptk_udp_socket_send_to: exit");
        return PTK_OK;
    }
    
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        debug("sendto() would block, registering for write event");
        event_registration_add(sock->event_loop->registrations, fd, current_threadlet, PTK_EVENT_WRITE, ptk_now_ms() + timeout_ms);
        platform_add_fd(sock->event_loop->platform, fd, PTK_EVENT_WRITE);
        timeout_heap_add(sock->event_loop->timeouts, fd, ptk_now_ms() + timeout_ms);
        debug("Yielding threadlet");
        ptk_threadlet_yield();
        
        if (current_threadlet->status == THREADLET_ABORTED) {
            warn("ptk_udp_socket_send_to: timeout");
            ptk_set_err(PTK_ERR_TIMEOUT);
            info("ptk_udp_socket_send_to: exit");
            return PTK_ERR_TIMEOUT;
        }
        
        debug("Resuming sendto after yield");
        info("ptk_udp_socket_send_to: exit");
        return ptk_udp_socket_send_to(sock, data, dest_addr, broadcast, timeout_ms);
    }
    
    warn("sendto() failed: %s", strerror(errno));
    ptk_set_err(PTK_ERR_NETWORK_ERROR);
    info("ptk_udp_socket_send_to: exit");
    return PTK_ERR_NETWORK_ERROR;
}

/**
 * Receive a single UDP packet from any address (blocking).
 * 
 * This function receives a single UDP packet and returns it as a buffer.
 * It fills the sender_addr parameter with the source address of the received packet if provided.
 * If no data is available, it registers the socket for read events and yields the current threadlet.
 * When data is available or the timeout expires, it resumes and attempts to read again.
 * If the read operation times out, it returns NULL. If there is a network error, it returns NULL.
 * If the socket is invalid or arguments are bad, it returns NULL.
 * If the socket was aborted while waiting, it returns NULL (handled by threadlet status).
 * The returned buffer must be freed by the caller using ptk_free().
 * @param sock UDP socket (must be valid and of type PTK_SOCK_UDP).
 * @param sender_addr Output parameter to receive the source address of the packet (can be NULL).
 * @param timeout_ms Timeout for the read operation (in milliseconds). If 0, wait indefinitely.
 * @return Buffer containing received packet on success (must be freed with ptk_free()), 
 *         NULL on error or timeout. Check ptk_get_err() for error details.
 */
ptk_buf *ptk_udp_socket_recv_from(ptk_sock *sock, ptk_address_t *sender_addr, ptk_duration_ms timeout_ms) {
    info("ptk_udp_socket_recv_from: entry");
    
    if (!sock || sock->type != PTK_SOCK_UDP) {
        warn("Invalid arguments to ptk_udp_socket_recv_from");
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return NULL;
    }
    
    int fd = sock->fd;
    
    // Create a buffer to receive data
    ptk_buf *packet_buf = ptk_buf_alloc(65535); // Max UDP packet size (buf_size_t is uint16_t)
    if (!packet_buf) {
        error("Failed to create packet buffer");
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return NULL;
    }
    
    struct sockaddr_in src_addr;
    socklen_t addr_len = sizeof(src_addr);
    debug("Calling recvfrom() on fd %d", fd);
    ssize_t bytes_read = recvfrom(fd, packet_buf->data, packet_buf->data_len, MSG_DONTWAIT, 
                                 (struct sockaddr *)&src_addr, &addr_len);
    
    if (bytes_read >= 0) {
        packet_buf->end = bytes_read;
        debug("Received %zd bytes in UDP packet", bytes_read);
        
        // Update sender address
        if (sender_addr) {
            sender_addr->ip = src_addr.sin_addr.s_addr;
            sender_addr->port = ntohs(src_addr.sin_port);
            sender_addr->family = AF_INET;
        }
        
        info("ptk_udp_socket_recv_from: exit");
        return packet_buf;
    }
    
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        debug("recvfrom() would block, registering for read event");
        event_registration_add(sock->event_loop->registrations, fd, current_threadlet, PTK_EVENT_READ, ptk_now_ms() + timeout_ms);
        platform_add_fd(sock->event_loop->platform, fd, PTK_EVENT_READ);
        timeout_heap_add(sock->event_loop->timeouts, fd, ptk_now_ms() + timeout_ms);
        debug("Yielding threadlet");
        ptk_threadlet_yield();
        
        if (current_threadlet->status == THREADLET_ABORTED) {
            warn("ptk_udp_socket_recv_from: timeout");
            ptk_free(&packet_buf);
            ptk_set_err(PTK_ERR_TIMEOUT);
            info("ptk_udp_socket_recv_from: exit");
            return NULL;
        }
        
        debug("Resuming recvfrom after yield");
        ptk_free(&packet_buf); // Free the temporary buffer and try again
        info("ptk_udp_socket_recv_from: exit");
        return ptk_udp_socket_recv_from(sock, sender_addr, timeout_ms);
    }
    
    warn("recvfrom() failed: %s", strerror(errno));
    ptk_free(&packet_buf);
    ptk_set_err(PTK_ERR_NETWORK_ERROR);
    info("ptk_udp_socket_recv_from: exit");
    return NULL;
}

//=============================================================================
// Multi-Packet UDP Functions (New API)
//=============================================================================

/**
 * Send multiple UDP packets to the same address using efficient multi-packet syscalls (blocking).
 * This function uses sendmmsg() on Linux for efficient batch sending.
 * @param sock UDP socket (must be valid and of type PTK_SOCK_UDP).
 * @param data_array Array of UDP buffer entries containing data and addresses to send.
 * @param dest_addr Destination address to send to (cannot be NULL).
 * @param broadcast If true, allows sending to broadcast addresses.
 * @param timeout_ms Timeout for the send operation (in milliseconds). If 0, wait indefinitely.
 * @return PTK_OK on success, PTK_ERR_TIMEOUT on timeout, PTK_ERR_NETWORK_ERROR on error (ptk_get_err() set).
 */
ptk_err ptk_udp_socket_send_many_to(ptk_sock *sock, ptk_udp_buf_entry_array_t *data_array, const ptk_address_t *dest_addr, bool broadcast, ptk_duration_ms timeout_ms) {
    info("ptk_udp_socket_send_many_to: entry");
    
    if (!sock || sock->type != PTK_SOCK_UDP || !data_array || !dest_addr) {
        warn("Invalid arguments to ptk_udp_socket_send_many_to");
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return PTK_ERR_INVALID_PARAM;
    }
    
    size_t array_len = ptk_udp_buf_entry_array_len(data_array);
    if (array_len == 0) {
        debug("Empty buffer array, nothing to send");
        info("ptk_udp_socket_send_many_to: exit");
        return PTK_OK;
    }
    
    int fd = sock->fd;
    
    // Prepare destination address
    struct sockaddr_in dest_sockaddr;
    memset(&dest_sockaddr, 0, sizeof(dest_sockaddr));
    dest_sockaddr.sin_family = AF_INET;
    dest_sockaddr.sin_addr.s_addr = dest_addr->ip;
    dest_sockaddr.sin_port = htons(dest_addr->port);
    
#ifdef __linux__
    // Use sendmmsg() on Linux for efficient batch sending
    struct mmsghdr *msgs = ptk_alloc(array_len * sizeof(struct mmsghdr), NULL);
    struct iovec *iov = ptk_alloc(array_len * sizeof(struct iovec), NULL);
    if (!msgs || !iov) {
        error("Failed to allocate message arrays");
        ptk_free(&msgs);
        ptk_free(&iov);
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return PTK_ERR_NO_RESOURCES;
    }
    
    // Fill message array
    for (size_t i = 0; i < array_len; i++) {
        ptk_udp_buf_entry_t entry;
        ptk_err err = ptk_udp_buf_entry_array_get(data_array, i, &entry);
        if (err != PTK_OK || !entry.buf) {
            warn("Failed to get buffer entry %zu from array", i);
            continue;
        }
        size_t data_len = entry.buf->end - entry.buf->start;
        iov[i].iov_base = entry.buf->data + entry.buf->start;
        iov[i].iov_len = data_len;
        
        memset(&msgs[i], 0, sizeof(msgs[i]));
        msgs[i].msg_hdr.msg_name = &dest_sockaddr;
        msgs[i].msg_hdr.msg_namelen = sizeof(dest_sockaddr);
        msgs[i].msg_hdr.msg_iov = &iov[i];
        msgs[i].msg_hdr.msg_iovlen = 1;
        
        debug("Packet %zu: %zu bytes", i, data_len);
    }
    
    debug("Using sendmmsg() to send %zu packets", array_len);
    int packets_sent = sendmmsg(fd, msgs, array_len, MSG_DONTWAIT);
    
    if (packets_sent >= 0) {
        debug("sendmmsg() sent %d packets", packets_sent);
        
        // Update buffer start positions for sent packets
        for (int i = 0; i < packets_sent; i++) {
            ptk_udp_buf_entry_t entry;
            ptk_err err = ptk_udp_buf_entry_array_get(data_array, i, &entry);
            if (err == PTK_OK && entry.buf) {
                entry.buf->start = entry.buf->end; // Mark as fully sent
                trace("Marked packet %d as sent", i);
            }
        }
        
        ptk_free(&msgs);
        ptk_free(&iov);
        info("ptk_udp_socket_send_many_to: exit");
        return PTK_OK;
    }
    
    ptk_free(&msgs);
    ptk_free(&iov);
    
#else
    // Fallback to individual sendto() calls on non-Linux systems
    debug("Using individual sendto() calls for %zu packets", array_len);
    
    for (size_t i = 0; i < array_len; i++) {
        ptk_udp_buf_entry_t *entry;
        ptk_err err = ptk_udp_buf_entry_array_get(data_array, i, &entry);
        if (err != PTK_OK || !entry || !entry->buf) {
            warn("Failed to get buffer entry %zu from array", i);
            continue;
        }
        
        size_t data_len = entry->buf->end - entry->buf->start;
        if (data_len == 0) continue;
        
        ssize_t bytes_sent = sendto(fd, entry->buf->data + entry->buf->start, data_len, MSG_DONTWAIT,
                                   (struct sockaddr *)&dest_sockaddr, sizeof(dest_sockaddr));
        
        if (bytes_sent >= 0) {
            entry->buf->start += bytes_sent; // Update buffer position
            debug("Sent packet %zu: %zd bytes", i, bytes_sent);
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            debug("sendto() would block on packet %zu", i);
            break; // Stop sending and register for write event
        } else {
            warn("sendto() failed on packet %zu: %s", i, strerror(errno));
            ptk_set_err(PTK_ERR_NETWORK_ERROR);
            info("ptk_udp_socket_send_many_to: exit");
            return PTK_ERR_NETWORK_ERROR;
        }
    }
    
    info("ptk_udp_socket_send_many_to: exit");
    return PTK_OK;
    
#endif
    
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        debug("send operation would block, registering for write event");
        event_registration_add(sock->event_loop->registrations, fd, current_threadlet, PTK_EVENT_WRITE, ptk_now_ms() + timeout_ms);
        platform_add_fd(sock->event_loop->platform, fd, PTK_EVENT_WRITE);
        timeout_heap_add(sock->event_loop->timeouts, fd, ptk_now_ms() + timeout_ms);
        debug("Yielding threadlet");
        ptk_threadlet_yield();
        
        if (current_threadlet->status == THREADLET_ABORTED) {
            warn("ptk_udp_socket_send_many_to: timeout");
            ptk_set_err(PTK_ERR_TIMEOUT);
            info("ptk_udp_socket_send_many_to: exit");
            return PTK_ERR_TIMEOUT;
        }
        
        debug("Resuming send after yield");
        info("ptk_udp_socket_send_many_to: exit");
        return ptk_udp_socket_send_many_to(sock, data_array, dest_addr, broadcast, timeout_ms);
    }
    
    warn("send operation failed: %s", strerror(errno));
    ptk_set_err(PTK_ERR_NETWORK_ERROR);
    info("ptk_udp_socket_send_many_to: exit");
    return PTK_ERR_NETWORK_ERROR;
}

/**
 * Receive multiple UDP packets from any address using efficient multi-packet syscalls (blocking).
 * This function uses recvmmsg() on Linux for efficient batch receiving.
 * @param sock UDP socket (must be valid and of type PTK_SOCK_UDP).
 * @param wait_for_packets If true, wait the entire timeout period and return all packets.
 *                        If false, return as soon as any packets are available.
 * @param timeout_ms Timeout for the read operation (in milliseconds). If 0, wait indefinitely.
 * @return Array of UDP buffer entries containing received packets on success (must be freed with ptk_free()), 
 *         NULL on error. Check ptk_get_err() for error details.
 */
ptk_udp_buf_entry_array_t *ptk_udp_socket_recv_many_from(ptk_sock *sock, bool wait_for_packets, ptk_duration_ms timeout_ms) {
    info("ptk_udp_socket_recv_many_from: entry");
    
    if (!sock || sock->type != PTK_SOCK_UDP) {
        warn("Invalid arguments to ptk_udp_socket_recv_many_from");
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return NULL;
    }
    
    int fd = sock->fd;
    ptk_time_ms start_time = ptk_now_ms();
    ptk_time_ms end_time = (timeout_ms == 0) ? PTK_TIME_WAIT_FOREVER : start_time + timeout_ms;
    
    // Create UDP buffer entry array to collect packets
    ptk_udp_buf_entry_array_t *packet_array = ptk_udp_buf_entry_array_create(16, NULL); // Start with space for 16 packets
    if (!packet_array) {
        error("Failed to create UDP buffer entry array");
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return NULL;
    }
    
#ifdef __linux__
    // Use recvmmsg() on Linux for efficient batch receiving
    const size_t MAX_MSGS = 16; // Receive up to 16 packets at once
    struct mmsghdr msgs[MAX_MSGS];
    struct iovec iov[MAX_MSGS];
    struct sockaddr_in src_addrs[MAX_MSGS];
    ptk_buf *buffers[MAX_MSGS];
    
    // Prepare message array
    for (size_t i = 0; i < MAX_MSGS; i++) {
        buffers[i] = ptk_buf_alloc(65535); // Max UDP packet size (buf_size_t is uint16_t)
        if (!buffers[i]) {
            error("Failed to allocate packet buffer %zu", i);
            // Clean up allocated buffers
            for (size_t j = 0; j < i; j++) {
                ptk_free(&buffers[j]);
            }
            ptk_free(&packet_array);
            ptk_set_err(PTK_ERR_NO_RESOURCES);
            return NULL;
        }
        
        iov[i].iov_base = buffers[i]->data;
        iov[i].iov_len = buffers[i]->data_len;
        
        memset(&msgs[i], 0, sizeof(msgs[i]));
        msgs[i].msg_hdr.msg_name = &src_addrs[i];
        msgs[i].msg_hdr.msg_namelen = sizeof(src_addrs[i]);
        msgs[i].msg_hdr.msg_iov = &iov[i];
        msgs[i].msg_hdr.msg_iovlen = 1;
    }
    
    while (true) {
        debug("Calling recvmmsg() for up to %zu packets", MAX_MSGS);
        int packets_received = recvmmsg(fd, msgs, MAX_MSGS, MSG_DONTWAIT, NULL);
        
        if (packets_received > 0) {
            debug("recvmmsg() received %d packets", packets_received);
            
            // Process received packets
            for (int i = 0; i < packets_received; i++) {
                buffers[i]->end = msgs[i].msg_len;
                
                // Create UDP buffer entry
                ptk_udp_buf_entry_t entry;
                entry.buf = buffers[i];
                entry.sender_addr.ip = src_addrs[i].sin_addr.s_addr;
                entry.sender_addr.port = ntohs(src_addrs[i].sin_port);
                entry.sender_addr.family = AF_INET;
                
                // Add to array
                ptk_err err = ptk_udp_buf_entry_array_append(packet_array, entry);
                if (err != PTK_OK) {
                    warn("Failed to append packet %d to array", i);
                    ptk_free(&buffers[i]);
                } else {
                    debug("Added packet %d: %u bytes from %u:%u", i, msgs[i].msg_len,
                          entry.sender_addr.ip, entry.sender_addr.port);
                }
                
                // Allocate new buffer for next iteration
                buffers[i] = ptk_buf_alloc(65535);
                if (!buffers[i]) {
                    warn("Failed to allocate replacement buffer %d", i);
                }
            }
            
            // If not waiting for multiple packets, return immediately
            if (!wait_for_packets && ptk_udp_buf_entry_array_len(packet_array) > 0) {
                debug("Returning immediately with %zu packets", ptk_udp_buf_entry_array_len(packet_array));
                // Clean up unused buffers
                for (size_t i = 0; i < MAX_MSGS; i++) {
                    if (buffers[i]) ptk_free(&buffers[i]);
                }
                info("ptk_udp_socket_recv_many_from: exit");
                return packet_array;
            }
            
            // Check timeout
            ptk_time_ms current_time = ptk_now_ms();
            if (timeout_ms != 0 && current_time >= end_time) {
                debug("Timeout reached, returning %zu packets", ptk_udp_buf_entry_array_len(packet_array));
                // Clean up unused buffers
                for (size_t i = 0; i < MAX_MSGS; i++) {
                    if (buffers[i]) ptk_free(&buffers[i]);
                }
                info("ptk_udp_socket_recv_many_from: exit");
                return packet_array;
            }
            
            continue; // Try to receive more packets
        }
        
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // No more packets available right now
            if (ptk_udp_buf_entry_array_len(packet_array) > 0 && !wait_for_packets) {
                debug("No more packets, returning %zu packets", ptk_udp_buf_entry_array_len(packet_array));
                // Clean up unused buffers
                for (size_t i = 0; i < MAX_MSGS; i++) {
                    if (buffers[i]) ptk_free(&buffers[i]);
                }
                info("ptk_udp_socket_recv_many_from: exit");
                return packet_array;
            }
            
            // Calculate remaining timeout and yield
            ptk_time_ms current_time = ptk_now_ms();
            if (timeout_ms != 0 && current_time >= end_time) {
                debug("Timeout reached, returning %zu packets", ptk_udp_buf_entry_array_len(packet_array));
                // Clean up unused buffers
                for (size_t i = 0; i < MAX_MSGS; i++) {
                    if (buffers[i]) ptk_free(&buffers[i]);
                }
                info("ptk_udp_socket_recv_many_from: exit");
                return packet_array;
            }
            
            ptk_duration_ms remaining_timeout = (timeout_ms == 0) ? 0 : end_time - current_time;
            debug("recvmmsg() would block, registering for read event (remaining timeout: %lld ms)", remaining_timeout);
            
            event_registration_add(sock->event_loop->registrations, fd, current_threadlet, PTK_EVENT_READ, 
                                 ptk_now_ms() + remaining_timeout);
            platform_add_fd(sock->event_loop->platform, fd, PTK_EVENT_READ);
            timeout_heap_add(sock->event_loop->timeouts, fd, ptk_now_ms() + remaining_timeout);
            debug("Yielding threadlet");
            ptk_threadlet_yield();
            
            if (current_threadlet->status == THREADLET_ABORTED) {
                debug("Timeout occurred, returning %zu packets", ptk_udp_buf_entry_array_len(packet_array));
                // Clean up unused buffers
                for (size_t i = 0; i < MAX_MSGS; i++) {
                    if (buffers[i]) ptk_free(&buffers[i]);
                }
                info("ptk_udp_socket_recv_many_from: exit");
                return packet_array;
            }
            
            debug("Resuming recvmmsg after yield");
            continue;
        }
        
        // Network error occurred
        warn("recvmmsg() failed: %s", strerror(errno));
        // Clean up unused buffers
        for (size_t i = 0; i < MAX_MSGS; i++) {
            if (buffers[i]) ptk_free(&buffers[i]);
        }
        ptk_free(&packet_array);
        ptk_set_err(PTK_ERR_NETWORK_ERROR);
        info("ptk_udp_socket_recv_many_from: exit");
        return NULL;
    }
    
#else
    // Fallback to individual recvfrom() calls on non-Linux systems
    debug("Using individual recvfrom() calls for packet reception");
    
    while (true) {
        ptk_buf *packet_buf = ptk_buf_alloc(65535); // Max UDP packet size (buf_size_t is uint16_t)
        if (!packet_buf) {
            error("Failed to create packet buffer");
            ptk_free(&packet_array);
            ptk_set_err(PTK_ERR_NO_RESOURCES);
            return NULL;
        }
        
        struct sockaddr_in src_addr;
        socklen_t addr_len = sizeof(src_addr);
        ssize_t bytes_read = recvfrom(fd, packet_buf->data, packet_buf->data_len, MSG_DONTWAIT, 
                                     (struct sockaddr *)&src_addr, &addr_len);
        
        if (bytes_read >= 0) {
            packet_buf->end = bytes_read;
            debug("Received %zd bytes in UDP packet", bytes_read);
            
            // Create UDP buffer entry
            ptk_udp_buf_entry_t entry;
            entry.buf = packet_buf;
            entry.sender_addr.ip = src_addr.sin_addr.s_addr;
            entry.sender_addr.port = ntohs(src_addr.sin_port);
            entry.sender_addr.family = AF_INET;
            
            // Add to array
            ptk_err err = ptk_udp_buf_entry_array_append(packet_array, &entry);
            if (err != PTK_OK) {
                warn("Failed to append packet to array");
                ptk_free(&packet_buf);
                ptk_free(&packet_array);
                ptk_set_err(err);
                return NULL;
            }
            
            // If not waiting for multiple packets, return immediately
            if (!wait_for_packets) {
                debug("Returning immediately with %zu packets", ptk_udp_buf_entry_array_len(packet_array));
                info("ptk_udp_socket_recv_many_from: exit");
                return packet_array;
            }
            
            // Check timeout
            ptk_time_ms current_time = ptk_now_ms();
            if (timeout_ms != 0 && current_time >= end_time) {
                debug("Timeout reached, returning %zu packets", ptk_udp_buf_entry_array_len(packet_array));
                info("ptk_udp_socket_recv_many_from: exit");
                return packet_array;
            }
            
            continue; // Try to receive more packets
        }
        
        // Free unused packet buffer
        ptk_free(&packet_buf);
        
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // No more packets available right now
            if (ptk_udp_buf_entry_array_len(packet_array) > 0 && !wait_for_packets) {
                debug("No more packets, returning %zu packets", ptk_udp_buf_entry_array_len(packet_array));
                info("ptk_udp_socket_recv_many_from: exit");
                return packet_array;
            }
            
            // Calculate remaining timeout and yield
            ptk_time_ms current_time = ptk_now_ms();
            if (timeout_ms != 0 && current_time >= end_time) {
                debug("Timeout reached, returning %zu packets", ptk_udp_buf_entry_array_len(packet_array));
                info("ptk_udp_socket_recv_many_from: exit");
                return packet_array;
            }
            
            ptk_duration_ms remaining_timeout = (timeout_ms == 0) ? 0 : end_time - current_time;
            debug("recvfrom() would block, registering for read event (remaining timeout: %lld ms)", remaining_timeout);
            
            event_registration_add(sock->event_loop->registrations, fd, current_threadlet, PTK_EVENT_READ, 
                                 ptk_now_ms() + remaining_timeout);
            platform_add_fd(sock->event_loop->platform, fd, PTK_EVENT_READ);
            timeout_heap_add(sock->event_loop->timeouts, fd, ptk_now_ms() + remaining_timeout);
            debug("Yielding threadlet");
            ptk_threadlet_yield();
            
            if (current_threadlet->status == THREADLET_ABORTED) {
                debug("Timeout occurred, returning %zu packets", ptk_udp_buf_entry_array_len(packet_array));
                info("ptk_udp_socket_recv_many_from: exit");
                return packet_array;
            }
            
            debug("Resuming recvfrom after yield");
            continue;
        }
        
        // Network error occurred
        warn("recvfrom() failed: %s", strerror(errno));
        ptk_free(&packet_array);
        ptk_set_err(PTK_ERR_NETWORK_ERROR);
        info("ptk_udp_socket_recv_many_from: exit");
        return NULL;
    }
#endif
}
