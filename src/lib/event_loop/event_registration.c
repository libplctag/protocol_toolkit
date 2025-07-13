#include "event_registration.h"
#include <ptk_alloc.h>
#include <ptk_log.h>
#include <string.h>

// Hash function for file descriptors
static size_t hash_fd(int fd, size_t capacity) {
    // Simple hash - fd values are usually small integers
    return ((size_t)fd * 2654435761U) % capacity;
}

// Find slot for fd (existing or empty)
static event_registration_t *find_slot(event_registration_table_t *table, int fd) {
    size_t start_index = hash_fd(fd, table->capacity);
    size_t index = start_index;
    
    do {
        event_registration_t *entry = &table->entries[index];
        if (!entry->in_use || entry->fd == fd) {
            return entry;
        }
        index = (index + 1) % table->capacity;
    } while (index != start_index);
    
    return NULL; // Table full
}

// Destructor for registration table
static void event_registration_table_destructor(void *ptr) {
    event_registration_table_t *table = (event_registration_table_t *)ptr;
    if (table && table->entries) {
        ptk_free(table->entries);
    }
}

event_registration_table_t *event_registration_table_create(size_t initial_capacity) {
    if (initial_capacity == 0) {
        initial_capacity = 64; // Default capacity
    }
    
    event_registration_table_t *table = ptk_alloc(sizeof(event_registration_table_t), 
                                                  event_registration_table_destructor);
    if (!table) {
        warn("Failed to allocate event registration table");
        return NULL;
    }
    
    table->entries = ptk_alloc(initial_capacity * sizeof(event_registration_t), NULL);
    if (!table->entries) {
        warn("Failed to allocate event registration entries");
        ptk_free(table);
        return NULL;
    }
    
    memset(table->entries, 0, initial_capacity * sizeof(event_registration_t));
    table->capacity = initial_capacity;
    table->count = 0;
    
    trace("Created event registration table with capacity %zu", initial_capacity);
    return table;
}

void event_registration_table_destroy(event_registration_table_t *table) {
    if (table) {
        trace("Destroying event registration table (count=%zu)", table->count);
        ptk_free(table);
    }
}

ptk_err event_registration_add(event_registration_table_t *table, 
                              int fd, 
                              threadlet_t *threadlet, 
                              uint32_t events, 
                              ptk_time_ms deadline) {
    if (!table || fd < 0 || !threadlet) {
        warn("Invalid arguments to event_registration_add");
        return PTK_ERR_INVALID_ARGUMENT;
    }
    
    // Check if table is getting full (>75% capacity)
    if (table->count * 4 >= table->capacity * 3) {
        warn("Event registration table is nearly full (%zu/%zu)", table->count, table->capacity);
        return PTK_ERR_NO_MEMORY;
    }
    
    event_registration_t *slot = find_slot(table, fd);
    if (!slot) {
        warn("No available slot in event registration table");
        return PTK_ERR_NO_MEMORY;
    }
    
    // If slot was empty, increment count
    if (!slot->in_use) {
        table->count++;
    }
    
    slot->fd = fd;
    slot->waiting_threadlet = threadlet;
    slot->events = events;
    slot->deadline = deadline;
    slot->in_use = true;
    
    trace("Added event registration: fd=%d, events=0x%x, deadline=%lu", 
          fd, events, deadline);
    return PTK_OK;
}

event_registration_t *event_registration_lookup(event_registration_table_t *table, int fd) {
    if (!table || fd < 0) {
        return NULL;
    }
    
    event_registration_t *slot = find_slot(table, fd);
    if (slot && slot->in_use && slot->fd == fd) {
        return slot;
    }
    
    return NULL;
}

void event_registration_remove(event_registration_table_t *table, int fd) {
    if (!table || fd < 0) {
        return;
    }
    
    event_registration_t *slot = find_slot(table, fd);
    if (slot && slot->in_use && slot->fd == fd) {
        trace("Removing event registration: fd=%d", fd);
        memset(slot, 0, sizeof(event_registration_t));
        table->count--;
    }
}

size_t event_registration_table_count(const event_registration_table_t *table) {
    return table ? table->count : 0;
}

bool event_registration_table_is_full(const event_registration_table_t *table) {
    if (!table) return true;
    return table->count * 4 >= table->capacity * 3; // >75% full
}
