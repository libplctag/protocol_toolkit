/**
 * PTK - POSIX Socket Implementation
 * 
 * POSIX-compliant socket operations for TCP, UDP, and serial connections.
 */

#include "ptk_connection.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <termios.h>

/* Forward declarations for error handling */
extern void ptk_clear_error(void);
extern void ptk_set_error_internal_internal(ptk_status_t error);


/**
 * Convert errno to PTK status code
 */
static ptk_status_t errno_to_ptk_status(void) {
    switch (errno) {
        case ECONNREFUSED:
        case ECONNRESET:
        case ENOTCONN:
            return PTK_ERROR_NOT_CONNECTED;
        case ETIMEDOUT:
            return PTK_ERROR_TIMEOUT;
        case EINVAL:
            return PTK_ERROR_INVALID_PARAM;
        case ENOMEM:
        case ENOBUFS:
            return PTK_ERROR_OUT_OF_MEMORY;
        case EINTR:
            return PTK_ERROR_INTERRUPTED;
        default:
            return PTK_ERROR_CONNECT;  /* Generic connection error */
    }
}

/**
 * Set socket to non-blocking mode
 */
static ptk_status_t set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        return errno_to_ptk_status();
    }
    
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        return errno_to_ptk_status();
    }
    
    return PTK_OK;
}

/**
 * Resolve hostname to IP address
 */
static ptk_status_t resolve_hostname(const char* host, struct sockaddr_in* addr) {
    /* Try direct IP address first */
    if (inet_pton(AF_INET, host, &addr->sin_addr) == 1) {
        return PTK_OK;
    }
    
    /* Try DNS resolution */
    struct hostent* he = gethostbyname(host);
    if (!he) {
        return PTK_ERROR_DNS_RESOLVE;
    }
    
    memcpy(&addr->sin_addr, he->h_addr, he->h_length);
    return PTK_OK;
}

/**
 * Convert baud rate to termios constant
 */
static speed_t baud_to_speed(int baud_rate) {
    switch (baud_rate) {
        case 9600: return B9600;
        case 19200: return B19200;
        case 38400: return B38400;
        case 57600: return B57600;
        case 115200: return B115200;
        default: return B9600;  /* Default fallback */
    }
}

/**
 * Initialize TCP connection
 */
extern ptk_status_t ptk_init_tcp_connection(ptk_tcp_connection_t* conn, const char* host, uint16_t port) {
    if (!conn || !host || port == 0) {
        ptk_set_error_internal(PTK_ERROR_INVALID_PARAM);
        return PTK_ERROR_INVALID_PARAM;
    }
    
    /* Initialize connection structure */
    memset(conn, 0, sizeof(*conn));
    conn->base.type = PTK_EVENT_SOURCE_TCP;
    conn->base.state = 0;
    conn->connect_timeout_ms = 5000;  /* Default 5 second timeout */
    
    /* Create socket */
    conn->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (conn->fd == -1) {
        ptk_status_t status = errno_to_ptk_status();
        ptk_set_error_internal(status);
        return status;
    }
    
    /* Set up address structure */
    conn->addr.sin_family = AF_INET;
    conn->addr.sin_port = htons(port);
    
    /* Resolve hostname */
    ptk_status_t status = resolve_hostname(host, &conn->addr);
    if (status != PTK_OK) {
        close(conn->fd);
        conn->fd = -1;
        ptk_set_error_internal(status);
        return status;
    }
    
    /* Set socket to non-blocking for asynchronous operations */
    status = set_nonblocking(conn->fd);
    if (status != PTK_OK) {
        close(conn->fd);
        conn->fd = -1;
        ptk_set_error_internal(status);
        return status;
    }
    
    /* Attempt connection */
    if (connect(conn->fd, (struct sockaddr*)&conn->addr, sizeof(conn->addr)) == -1) {
        if (errno != EINPROGRESS) {
            status = errno_to_ptk_status();
            close(conn->fd);
            conn->fd = -1;
            ptk_set_error_internal(status);
            return status;
        }
        /* Connection in progress - will complete asynchronously */
    }
    
    ptk_clear_error();
    return PTK_OK;
}

/**
 * Initialize UDP connection
 */
extern ptk_status_t ptk_init_udp_connection(ptk_udp_connection_t* conn, const char* host, uint16_t port) {
    if (!conn || !host || port == 0) {
        ptk_set_error_internal(PTK_ERROR_INVALID_PARAM);
        return PTK_ERROR_INVALID_PARAM;
    }
    
    /* Initialize connection structure */
    memset(conn, 0, sizeof(*conn));
    conn->base.type = PTK_EVENT_SOURCE_UDP;
    conn->base.state = 0;
    conn->bind_timeout_ms = 1000;
    
    /* Create socket */
    conn->fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (conn->fd == -1) {
        ptk_status_t status = errno_to_ptk_status();
        ptk_set_error_internal(status);
        return status;
    }
    
    /* Set up address structure */
    conn->addr.sin_family = AF_INET;
    conn->addr.sin_port = htons(port);
    
    /* Resolve hostname */
    ptk_status_t status = resolve_hostname(host, &conn->addr);
    if (status != PTK_OK) {
        close(conn->fd);
        conn->fd = -1;
        ptk_set_error_internal(status);
        return status;
    }
    
    /* Set socket to non-blocking */
    status = set_nonblocking(conn->fd);
    if (status != PTK_OK) {
        close(conn->fd);
        conn->fd = -1;
        ptk_set_error_internal(status);
        return status;
    }
    
    ptk_clear_error();
    return PTK_OK;
}

/**
 * Initialize serial connection
 */
extern ptk_status_t ptk_init_serial_connection(ptk_serial_connection_t* conn, const char* device, int baud) {
    if (!conn || !device || baud <= 0) {
        ptk_set_error_internal(PTK_ERROR_INVALID_PARAM);
        return PTK_ERROR_INVALID_PARAM;
    }
    
    /* Initialize connection structure */
    memset(conn, 0, sizeof(*conn));
    conn->base.type = PTK_EVENT_SOURCE_SERIAL;
    conn->base.state = 0;
    conn->baud_rate = baud;
    conn->read_timeout_ms = 1000;
    
    /* Copy device path (with bounds checking) */
    size_t device_len = strlen(device);
    if (device_len >= sizeof(conn->device_path)) {
        ptk_set_error_internal(PTK_ERROR_INVALID_PARAM);
        return PTK_ERROR_INVALID_PARAM;
    }
    strcpy(conn->device_path, device);
    
    /* Open serial device */
    conn->fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (conn->fd == -1) {
        ptk_status_t status = errno_to_ptk_status();
        ptk_set_error_internal(status);
        return status;
    }
    
    /* Configure serial port */
    struct termios tty;
    if (tcgetattr(conn->fd, &tty) != 0) {
        ptk_status_t status = errno_to_ptk_status();
        close(conn->fd);
        conn->fd = -1;
        ptk_set_error_internal(status);
        return status;
    }
    
    /* Set baud rate */
    speed_t speed = baud_to_speed(baud);
    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);
    
    /* Configure 8N1 mode */
    tty.c_cflag &= ~PARENB;   /* No parity */
    tty.c_cflag &= ~CSTOPB;   /* One stop bit */
    tty.c_cflag &= ~CSIZE;    /* Clear character size bits */
    tty.c_cflag |= CS8;       /* 8 data bits */
    tty.c_cflag &= ~CRTSCTS;  /* No hardware flow control */
    tty.c_cflag |= CREAD | CLOCAL;  /* Enable receiver and ignore modem lines */
    
    /* Set raw mode */
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_oflag &= ~OPOST;
    
    /* Apply settings */
    if (tcsetattr(conn->fd, TCSANOW, &tty) != 0) {
        ptk_status_t status = errno_to_ptk_status();
        close(conn->fd);
        conn->fd = -1;
        ptk_set_error_internal(status);
        return status;
    }
    
    ptk_clear_error();
    return PTK_OK;
}

/**
 * Read data from connection
 */
extern ptk_slice_t ptk_connection_read(ptk_event_source_t* conn, ptk_slice_t buffer, uint32_t timeout_ms) {
    if (!conn || !buffer.data || buffer.len == 0) {
        ptk_set_error_internal(PTK_ERROR_INVALID_PARAM);
        return (ptk_slice_t){.data = NULL, .len = 0};
    }
    
    int fd = -1;
    
    /* Get file descriptor based on connection type */
    switch (conn->type) {
        case PTK_EVENT_SOURCE_TCP:
            fd = ((ptk_tcp_connection_t*)conn)->fd;
            break;
        case PTK_EVENT_SOURCE_UDP:
            fd = ((ptk_udp_connection_t*)conn)->fd;
            break;
        case PTK_EVENT_SOURCE_SERIAL:
            fd = ((ptk_serial_connection_t*)conn)->fd;
            break;
        default:
            ptk_set_error_internal(PTK_ERROR_INVALID_PARAM);
            return (ptk_slice_t){.data = NULL, .len = 0};
    }
    
    if (fd == -1) {
        ptk_set_error_internal(PTK_ERROR_NOT_CONNECTED);
        return (ptk_slice_t){.data = NULL, .len = 0};
    }
    
    /* Attempt to read data */
    ssize_t bytes_read = recv(fd, buffer.data, buffer.len, MSG_DONTWAIT);
    
    if (bytes_read > 0) {
        /* Successful read */
        conn->state |= PTK_CONN_DATA_READY;
        ptk_clear_error();
        return (ptk_slice_t){.data = buffer.data, .len = (size_t)bytes_read};
    } else if (bytes_read == 0) {
        /* Connection closed */
        conn->state |= PTK_CONN_CLOSED;
        ptk_set_error_internal(PTK_ERROR_NOT_CONNECTED);
        return (ptk_slice_t){.data = NULL, .len = 0};
    } else {
        /* Error occurred */
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            /* No data available */
            ptk_clear_error();
            return (ptk_slice_t){.data = NULL, .len = 0};
        } else {
            conn->state |= PTK_CONN_ERROR;
            ptk_status_t status = errno_to_ptk_status();
            ptk_set_error_internal(status);
            return (ptk_slice_t){.data = NULL, .len = 0};
        }
    }
}

/**
 * Write data to connection
 */
extern ptk_status_t ptk_connection_write(ptk_event_source_t* conn, ptk_slice_t data, uint32_t timeout_ms) {
    if (!conn || !data.data || data.len == 0) {
        ptk_set_error_internal(PTK_ERROR_INVALID_PARAM);
        return PTK_ERROR_INVALID_PARAM;
    }
    
    int fd = -1;
    
    /* Get file descriptor based on connection type */
    switch (conn->type) {
        case PTK_EVENT_SOURCE_TCP:
            fd = ((ptk_tcp_connection_t*)conn)->fd;
            break;
        case PTK_EVENT_SOURCE_UDP:
            fd = ((ptk_udp_connection_t*)conn)->fd;
            break;
        case PTK_EVENT_SOURCE_SERIAL:
            fd = ((ptk_serial_connection_t*)conn)->fd;
            break;
        default:
            ptk_set_error_internal(PTK_ERROR_INVALID_PARAM);
            return PTK_ERROR_INVALID_PARAM;
    }
    
    if (fd == -1) {
        ptk_set_error_internal(PTK_ERROR_NOT_CONNECTED);
        return PTK_ERROR_NOT_CONNECTED;
    }
    
    /* Attempt to write data */
    ssize_t bytes_written = send(fd, data.data, data.len, MSG_DONTWAIT);
    
    if (bytes_written > 0) {
        /* Successful write */
        conn->state |= PTK_CONN_WRITE_READY;
        ptk_clear_error();
        return PTK_OK;
    } else if (bytes_written == 0) {
        /* Connection closed */
        conn->state |= PTK_CONN_CLOSED;
        ptk_set_error_internal(PTK_ERROR_NOT_CONNECTED);
        return PTK_ERROR_NOT_CONNECTED;
    } else {
        /* Error occurred */
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            /* Socket not ready for write */
            ptk_clear_error();
            return PTK_ERROR_TIMEOUT;
        } else {
            conn->state |= PTK_CONN_ERROR;
            ptk_status_t status = errno_to_ptk_status();
            ptk_set_error_internal(status);
            return status;
        }
    }
}

/**
 * Close connection
 */
extern ptk_status_t ptk_connection_close(ptk_event_source_t* conn) {
    if (!conn) {
        ptk_set_error_internal(PTK_ERROR_INVALID_PARAM);
        return PTK_ERROR_INVALID_PARAM;
    }
    
    int fd = -1;
    
    /* Get file descriptor based on connection type */
    switch (conn->type) {
        case PTK_EVENT_SOURCE_TCP:
            fd = ((ptk_tcp_connection_t*)conn)->fd;
            ((ptk_tcp_connection_t*)conn)->fd = -1;
            break;
        case PTK_EVENT_SOURCE_UDP:
            fd = ((ptk_udp_connection_t*)conn)->fd;
            ((ptk_udp_connection_t*)conn)->fd = -1;
            break;
        case PTK_EVENT_SOURCE_SERIAL:
            fd = ((ptk_serial_connection_t*)conn)->fd;
            ((ptk_serial_connection_t*)conn)->fd = -1;
            break;
        default:
            ptk_set_error_internal(PTK_ERROR_INVALID_PARAM);
            return PTK_ERROR_INVALID_PARAM;
    }
    
    if (fd != -1) {
        close(fd);
    }
    
    conn->state |= PTK_CONN_CLOSED;
    ptk_clear_error();
    return PTK_OK;
}