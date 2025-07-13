#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <ptk_utils.h>

// Forward declarations
typedef struct threadlet_t threadlet_t;

/**
 * @brief Registration entry tracking which threadlet is waiting on which file descriptor
 */
typedef struct event_registration_t {
    int fd;                           // File descriptor being monitored
    threadlet_t *waiting_threadlet;   // Threadlet waiting on this FD
    uint32_t events;                  // EPOLLIN/EPOLLOUT/etc events
    ptk_time_ms deadline;             // Timeout deadline (0 = no timeout)
    bool in_use;                      // Whether this slot is occupied
} event_registration_t;

/**
 * @brief Hash table for fast FD -> registration lookup
 */
typedef struct event_registration_table_t {
    event_registration_t *entries;    // Array of registration entries
    size_t capacity;                  // Total slots in hash table
    size_t count;                     // Number of active registrations
} event_registration_table_t;

// Table management
event_registration_table_t *event_registration_table_create(size_t initial_capacity);
void event_registration_table_destroy(event_registration_table_t *table);

// Registration operations
ptk_err event_registration_add(event_registration_table_t *table, 
                              int fd, 
                              threadlet_t *threadlet, 
                              uint32_t events, 
                              ptk_time_ms deadline);

event_registration_t *event_registration_lookup(event_registration_table_t *table, int fd);
void event_registration_remove(event_registration_table_t *table, int fd);

// Utilities
size_t event_registration_table_count(const event_registration_table_t *table);
bool event_registration_table_is_full(const event_registration_table_t *table);
