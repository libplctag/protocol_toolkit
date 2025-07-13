#include "timeout_heap.h"
#include <ptk_alloc.h>
#include <ptk_log.h>
#include <string.h>

// Helper macros for heap navigation
#define PARENT(i) (((i) - 1) / 2)
#define LEFT_CHILD(i) (2 * (i) + 1)
#define RIGHT_CHILD(i) (2 * (i) + 2)

// Swap two heap entries
static void swap_entries(timeout_entry_t *a, timeout_entry_t *b) {
    timeout_entry_t temp = *a;
    *a = *b;
    *b = temp;
}

// Restore heap property upward from given index
static void heapify_up(timeout_heap_t *heap, size_t index) {
    while (index > 0) {
        size_t parent = PARENT(index);
        if (heap->entries[index].deadline >= heap->entries[parent].deadline) {
            break; // Heap property satisfied
        }
        swap_entries(&heap->entries[index], &heap->entries[parent]);
        index = parent;
    }
}

// Restore heap property downward from given index
static void heapify_down(timeout_heap_t *heap, size_t index) {
    while (LEFT_CHILD(index) < heap->count) {
        size_t left = LEFT_CHILD(index);
        size_t right = RIGHT_CHILD(index);
        size_t smallest = index;
        
        // Find smallest among index, left child, right child
        if (heap->entries[left].deadline < heap->entries[smallest].deadline) {
            smallest = left;
        }
        if (right < heap->count && 
            heap->entries[right].deadline < heap->entries[smallest].deadline) {
            smallest = right;
        }
        
        if (smallest == index) {
            break; // Heap property satisfied
        }
        
        swap_entries(&heap->entries[index], &heap->entries[smallest]);
        index = smallest;
    }
}

// Find index of entry with given fd
static size_t find_entry_index(timeout_heap_t *heap, int fd) {
    for (size_t i = 0; i < heap->count; i++) {
        if (heap->entries[i].fd == fd) {
            return i;
        }
    }
    return SIZE_MAX; // Not found
}

// Destructor for timeout heap
static void timeout_heap_destructor(void *ptr) {
    timeout_heap_t *heap = (timeout_heap_t *)ptr;
    if (heap && heap->entries) {
        ptk_free(heap->entries);
    }
}

timeout_heap_t *timeout_heap_create(size_t initial_capacity) {
    if (initial_capacity == 0) {
        initial_capacity = 32; // Default capacity
    }
    
    timeout_heap_t *heap = ptk_alloc(sizeof(timeout_heap_t), timeout_heap_destructor);
    if (!heap) {
        warn("Failed to allocate timeout heap");
        return NULL;
    }
    
    heap->entries = ptk_alloc(initial_capacity * sizeof(timeout_entry_t), NULL);
    if (!heap->entries) {
        warn("Failed to allocate timeout heap entries");
        ptk_free(heap);
        return NULL;
    }
    
    heap->capacity = initial_capacity;
    heap->count = 0;
    
    trace("Created timeout heap with capacity %zu", initial_capacity);
    return heap;
}

void timeout_heap_destroy(timeout_heap_t *heap) {
    if (heap) {
        trace("Destroying timeout heap (count=%zu)", heap->count);
        ptk_free(heap);
    }
}

ptk_err timeout_heap_add(timeout_heap_t *heap, int fd, ptk_time_ms deadline) {
    if (!heap || fd < 0) {
        warn("Invalid arguments to timeout_heap_add");
        return PTK_ERR_INVALID_ARGUMENT;
    }
    
    // Check if we need to grow the heap
    if (heap->count >= heap->capacity) {
        warn("Timeout heap is full (%zu/%zu)", heap->count, heap->capacity);
        return PTK_ERR_NO_MEMORY;
    }
    
    // Add new entry at end and heapify up
    size_t index = heap->count++;
    heap->entries[index].fd = fd;
    heap->entries[index].deadline = deadline;
    
    heapify_up(heap, index);
    
    trace("Added timeout: fd=%d, deadline=%lu", fd, deadline);
    return PTK_OK;
}

timeout_entry_t *timeout_heap_peek(timeout_heap_t *heap) {
    if (!heap || heap->count == 0) {
        return NULL;
    }
    return &heap->entries[0]; // Root has earliest deadline
}

timeout_entry_t *timeout_heap_pop(timeout_heap_t *heap) {
    if (!heap || heap->count == 0) {
        return NULL;
    }
    
    static timeout_entry_t result; // Static to return pointer safely
    result = heap->entries[0];
    
    // Move last element to root and heapify down
    heap->entries[0] = heap->entries[--heap->count];
    if (heap->count > 0) {
        heapify_down(heap, 0);
    }
    
    trace("Popped timeout: fd=%d, deadline=%lu", result.fd, result.deadline);
    return &result;
}

void timeout_heap_remove(timeout_heap_t *heap, int fd) {
    if (!heap || fd < 0) {
        return;
    }
    
    size_t index = find_entry_index(heap, fd);
    if (index == SIZE_MAX) {
        return; // Not found
    }
    
    trace("Removing timeout: fd=%d", fd);
    
    // Move last element to this position
    heap->entries[index] = heap->entries[--heap->count];
    
    if (index < heap->count) {
        // Restore heap property (may need to go up or down)
        if (index > 0 && 
            heap->entries[index].deadline < heap->entries[PARENT(index)].deadline) {
            heapify_up(heap, index);
        } else {
            heapify_down(heap, index);
        }
    }
}

bool timeout_heap_is_empty(const timeout_heap_t *heap) {
    return !heap || heap->count == 0;
}

size_t timeout_heap_count(const timeout_heap_t *heap) {
    return heap ? heap->count : 0;
}

ptk_time_ms timeout_heap_next_deadline(const timeout_heap_t *heap) {
    if (!heap || heap->count == 0) {
        return 0; // No timeouts
    }
    return heap->entries[0].deadline;
}
