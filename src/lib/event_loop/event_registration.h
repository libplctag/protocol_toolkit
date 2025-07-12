#pragma once

#include <stdint.h>
#include <stdbool.h>

// Registration for a socket/file descriptor
typedef struct event_registration_t {
    int fd;
    void *waiting_threadlet;
    uint32_t events; // PTK_EVENT_READ | PTK_EVENT_WRITE
    uint64_t deadline;
    struct event_registration_t *next;
} event_registration_t;

// Hash table for fd -> registration
typedef struct event_registration_table_t event_registration_table_t;

event_registration_table_t *event_registration_table_create(void);
void event_registration_table_destroy(event_registration_table_t *table);
event_registration_t *event_registration_lookup(event_registration_table_t *table, int fd);
ptk_err event_registration_add(event_registration_table_t *table, int fd, void *threadlet, uint32_t events, uint64_t deadline);
ptk_err event_registration_remove(event_registration_table_t *table, int fd);
