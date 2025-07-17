#pragma once

/**
 * @file ptk_config.h
 * @brief Simple command line configuration parser
 *
 * Provides a lightweight configuration system for parsing command line arguments.
 * Designed to be simple and embedded-friendly.
 */

#include <stdint.h>
#include <stdbool.h>
#include <ptk_defs.h>
#include <ptk_err.h>

/**
 * Configuration field types
 */
typedef enum {
    PTK_CONFIG_STRING,   // char*
    PTK_CONFIG_INT,      // int*
    PTK_CONFIG_UINT16,   // uint16_t*
    PTK_CONFIG_BOOL,     // bool*
    PTK_CONFIG_HELP      // Special type for help flag
} ptk_config_type_t;

/**
 * Configuration field definition
 */
typedef struct {
    const char *name;           // Long option name (without --)
    char short_name;            // Short option character (0 if none)
    ptk_config_type_t type;     // Type of the field
    void *target;               // Pointer to store the value
    const char *help;           // Help text
    const char *default_str;    // Default value as string (for display)
} ptk_config_field_t;

/**
 * Sentinel value to mark end of configuration array
 */
#define PTK_CONFIG_END {NULL, 0, PTK_CONFIG_HELP, NULL, NULL, NULL}

/**
 * Parse command line arguments using field definitions
 *
 * @param argc Argument count
 * @param argv Argument array
 * @param fields Array of field definitions (terminated with PTK_CONFIG_END)
 * @param program_name Program name for help text (can be NULL to use argv[0])
 * @return 0 on success, 1 if help was shown, -1 on error
 */
PTK_API ptk_err_t ptk_config_parse(int argc, char *argv[], const ptk_config_field_t *fields, const char *program_name);

/**
 * Print help for configuration fields
 *
 * @param program_name Program name
 * @param fields Array of field definitions
 * @param description Optional program description
 */
PTK_API ptk_err_t ptk_config_print_help(const char *program_name, const ptk_config_field_t *fields, const char *description);
