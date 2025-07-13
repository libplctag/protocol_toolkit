
#pragma once

/**
 * @file ptk.h
 * @brief Protocol Toolkit global initialization and shutdown API
 *
 * This header provides the entry points for initializing and shutting down the Protocol Toolkit library.
 * You must call ptk_startup() before using any PTK functionality, and ptk_shutdown() when your application exits.
 * These functions set up and tear down platform-specific resources and global state for all PTK modules.
 *
 * Usage:
 *   if(ptk_startup() != PTK_OK) {
 *       // handle error
 *   }
 *   // ... use PTK APIs ...
 *   ptk_shutdown();
 */

#include <ptk_err.h>

/**
 * @brief Initialize the PTK library
 * 
 * This function must be called once before using any PTK functionality.
 * It initializes platform-specific resources and global state.
 * 
 * @return PTK_OK on success, error code on failure
 */
ptk_err ptk_startup(void);

/**
 * @brief Shutdown the PTK library
 * 
 * This function should be called once when the application is shutting down.
 * It cleans up platform-specific resources and global state.
 * 
 * @return PTK_OK on success, error code on failure
 */
ptk_err ptk_shutdown(void);