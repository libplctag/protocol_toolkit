#ifndef PTK_H
#define PTK_H

/**
 * PTK - Protocol Toolkit
 * 
 * Main umbrella header that includes all PTK components.
 * Include this single header to get access to all PTK functionality.
 */

#include "ptk_types.h"
#include "ptk_slice.h"
#include "ptk_scratch.h"
#include "ptk_connection.h"
#include "ptk_event.h"
#include "ptk_serialization.h"
#include "ptk_protocols.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * PTK Library Information
 */
#define PTK_VERSION_MAJOR 1
#define PTK_VERSION_MINOR 0
#define PTK_VERSION_PATCH 0

/**
 * Initialize the PTK library
 * Call this once before using any other PTK functions
 */
ptk_status_t ptk_init(void);

/**
 * Cleanup the PTK library
 * Call this to release any global resources
 */
void ptk_cleanup(void);

/**
 * Get the last error for the current thread
 * PTK functions store errors in thread-local storage for composability
 */
ptk_status_t ptk_get_last_error(void);

/**
 * Clear the last error for the current thread
 */
void ptk_clear_error(void);

#ifdef __cplusplus
}
#endif

#endif /* PTK_H */