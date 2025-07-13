#pragma once

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