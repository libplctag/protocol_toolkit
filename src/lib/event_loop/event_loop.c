// src/lib/event_loop/event_loop.c

#include "event_loop.h"
#include "threadlet.h"
#include "event_registration.h"
#include "timeout_heap.h"
#include "platform/linux_event_loop.h"

void ptk_event_loop_run(ptk_event_loop_t *loop) {
    loop->running = true;
    while (loop->running) {
        // 1. Poll for I/O events
        platform_event_list_t ready_events;
        ready_events.events = malloc(loop->platform->impl->max_events * sizeof(platform_event_t));
        int n = platform_poll_events(loop->platform, &ready_events, calculate_next_timeout(loop));
        
        // 2. Process ready events
        for (int i = 0; i < n; ++i) {
            int fd = ready_events.events[i].fd;
            event_registration_t *reg = event_registration_lookup(loop->registrations, fd);
            if (reg && reg->waiting_threadlet) {
                // Move threadlet from waiting to ready queue
                threadlet_queue_enqueue(&loop->ready_queue, reg->waiting_threadlet);
                event_registration_remove(loop->registrations, fd);
                platform_remove_fd(loop->platform, fd);
            }
        }
        free(ready_events.events);

        // 3. Process timeouts
        ptk_time_ms now = ptk_now_ms();
        while (!timeout_heap_is_empty(loop->timeout_heap)) {
            timeout_entry_t *entry = timeout_heap_peek(loop->timeout_heap);
            if (entry->deadline > now) break;
            event_registration_t *reg = event_registration_lookup(loop->registrations, entry->fd);
            if (reg && reg->waiting_threadlet) {
                // Timeout occurred
                reg->waiting_threadlet->status = THREADLET_TIMEOUT;
                threadlet_queue_enqueue(&loop->ready_queue, reg->waiting_threadlet);
                event_registration_remove(loop->registrations, entry->fd);
                platform_remove_fd(loop->platform, entry->fd);
            }
            timeout_heap_remove(loop->timeout_heap, entry->fd);
        }

        // 4. Run ready threadlets
        while (!threadlet_queue_is_empty(&loop->ready_queue)) {
            threadlet_t *t = threadlet_queue_dequeue(&loop->ready_queue);
            ptk_threadlet_resume(t);
            // If threadlet yields for I/O, it will be re-queued by socket API
            // If threadlet finishes, clean up
            if (t->status == THREADLET_FINISHED) {
                threadlet_destroy(t);
            }
        }
    }
}