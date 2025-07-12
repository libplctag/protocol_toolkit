#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <ptk_err.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file ptk_waitable.h
 * @brief Generic waitable interface for synchronous event loop operations.
 *
 * Implements a mechanism for objects (sockets, timers, etc.) that can be waited on for events,
 * and can be signaled or aborted from external threads.
 *
 * All blocking calls should use this internally.
 */

typedef struct ptk_waitable ptk_waitable_t;

/**
 * Status returned by waitable operations.
 */
typedef enum {
    PTK_WAIT_OK,        // Expected event occurred
    PTK_WAIT_SIGNAL,    // External signal (e.g., timer, user event)
    PTK_WAIT_TIMEOUT,   // Timeout expired
    PTK_WAIT_ERROR      // Any error (abort, closed, network, etc.)
} ptk_wait_status;

/**
 * Wait for the waitable to be signaled or timeout/error.
 *
 * @param waitable The waitable object.
 * @param timeout_ms Timeout in milliseconds (0 for infinite).
 * @return Status (see ptk_wait_status). If PTK_WAIT_ERROR, call ptk_get_err() to get details.
 */
ptk_wait_status ptk_waitable_wait(ptk_waitable_t *waitable, uint32_t timeout_ms);

/**
 * Signal the waitable, causing any blocking wait to return PTK_WAIT_SIGNAL.
 *
 * @param waitable The waitable to signal.
 * @return PTK_OK on success, error code on failure.
 */
ptk_err ptk_waitable_signal(ptk_waitable_t *waitable);

/**
 * Abort any ongoing wait on the waitable.
 * Blocking calls will return PTK_WAIT_ERROR, and ptk_get_err() will be set to PTK_ERR_ABORT.
 *
 * @param waitable The waitable to abort.
 * @return PTK_OK on success, error code on failure.
 */
ptk_err ptk_waitable_abort(ptk_waitable_t *waitable);

#ifdef __cplusplus
}
#endif
