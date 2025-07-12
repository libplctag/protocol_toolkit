#include "ptk_config.h"
#include "ptk_log.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//=============================================================================
// INTERNAL HELPERS
//=============================================================================

static int set_field_value(const ptk_config_field_t *field, const char *value) {
    if(!field->target) { return 0; }

    switch(field->type) {
        case PTK_CONFIG_STRING: {
            char **target = (char **)field->target;
            *target = (char *)value;  // Note: points to argv, not copied
            break;
        }
        case PTK_CONFIG_INT: {
            int *target = (int *)field->target;
            char *endptr;
            long val = strtol(value, &endptr, 10);
            if(*endptr != '\0') {
                error("Invalid integer value for --%s: %s", field->name, value);
                return -1;
            }
            *target = (int)val;
            break;
        }
        case PTK_CONFIG_UINT16: {
            uint16_t *target = (uint16_t *)field->target;
            char *endptr;
            unsigned long val = strtoul(value, &endptr, 10);
            if(*endptr != '\0' || val > UINT16_MAX) {
                error("Invalid uint16 value for --%s: %s", field->name, value);
                return -1;
            }
            *target = (uint16_t)val;
            break;
        }
        case PTK_CONFIG_BOOL: {
            bool *target = (bool *)field->target;
            if(strcmp(value, "true") == 0 || strcmp(value, "1") == 0) {
                *target = true;
            } else if(strcmp(value, "false") == 0 || strcmp(value, "0") == 0) {
                *target = false;
            } else {
                error("Invalid boolean value for --%s: %s (use true/false or 1/0)", field->name, value);
                return -1;
            }
            break;
        }
        case PTK_CONFIG_HELP:
            // Should not reach here
            break;
    }

    return 0;
}

static const ptk_config_field_t *find_field_by_name(const ptk_config_field_t *fields, const char *name) {
    for(const ptk_config_field_t *field = fields; field->name; field++) {
        if(strcmp(field->name, name) == 0) { return field; }
    }
    return NULL;
}

static const ptk_config_field_t *find_field_by_short(const ptk_config_field_t *fields, char short_name) {
    for(const ptk_config_field_t *field = fields; field->name; field++) {
        if(field->short_name == short_name) { return field; }
    }
    return NULL;
}

//=============================================================================
// PUBLIC API
//=============================================================================

void ptk_config_print_help(const char *program_name, const ptk_config_field_t *fields, const char *description) {
    printf("Usage: %s [OPTIONS]\n", program_name);
    if(description) { printf("%s\n\n", description); }

    printf("Options:\n");

    for(const ptk_config_field_t *field = fields; field->name; field++) {
        if(field->type == PTK_CONFIG_HELP) { continue; }

        printf("  ");

        if(field->short_name) {
            printf("-%c, ", field->short_name);
        } else {
            printf("    ");
        }

        printf("--%s", field->name);

        // Add value indicator for non-boolean types
        if(field->type != PTK_CONFIG_BOOL) {
            switch(field->type) {
                case PTK_CONFIG_STRING: printf("=STRING"); break;
                case PTK_CONFIG_INT: printf("=INTEGER"); break;
                case PTK_CONFIG_UINT16: printf("=NUMBER"); break;
                default: printf("=VALUE"); break;
            }
        }

        // Pad to align descriptions
        int padding = 25 - strlen(field->name) - (field->short_name ? 4 : 0);
        if(field->type != PTK_CONFIG_BOOL) {
            padding -= 8;  // Approximate length of value indicator
        }
        for(int i = 0; i < padding && i < 20; i++) { printf(" "); }

        printf("%s", field->help);

        if(field->default_str) { printf(" (default: %s)", field->default_str); }

        printf("\n");
    }

    printf("  -h, --help                   Show this help message\n");
}

int ptk_config_parse(int argc, char *argv[], const ptk_config_field_t *fields, const char *program_name) {
    if(!program_name) { program_name = argv[0]; }

    // Count fields and build getopt_long options array
    int field_count = 0;
    for(const ptk_config_field_t *field = fields; field->name; field++) { field_count++; }

    // Allocate option array (+2 for help and sentinel)
    struct option *long_options = malloc((field_count + 2) * sizeof(struct option));
    if(!long_options) {
        error("Failed to allocate options array");
        return -1;
    }

    // Build short options string
    char short_opts[256] = "h";  // Always include help
    int short_idx = 1;

    // Build long options and short options
    int opt_idx = 0;
    for(const ptk_config_field_t *field = fields; field->name; field++) {
        long_options[opt_idx].name = field->name;
        long_options[opt_idx].has_arg = (field->type == PTK_CONFIG_BOOL) ? no_argument : required_argument;
        long_options[opt_idx].flag = NULL;
        long_options[opt_idx].val = field->short_name ? field->short_name : (1000 + opt_idx);

        if(field->short_name && short_idx < sizeof(short_opts) - 2) {
            short_opts[short_idx++] = field->short_name;
            if(field->type != PTK_CONFIG_BOOL) { short_opts[short_idx++] = ':'; }
        }

        opt_idx++;
    }

    // Add help option
    long_options[opt_idx].name = "help";
    long_options[opt_idx].has_arg = no_argument;
    long_options[opt_idx].flag = NULL;
    long_options[opt_idx].val = 'h';
    opt_idx++;

    // Sentinel
    long_options[opt_idx] = (struct option){0, 0, 0, 0};

    short_opts[short_idx] = '\0';

    // Parse options
    int result = 0;
    int c;
    while((c = getopt_long(argc, argv, short_opts, long_options, NULL)) != -1) {
        if(c == 'h' || c == '?') {
            ptk_config_print_help(program_name, fields, NULL);
            result = 1;
            break;
        }

        const ptk_config_field_t *field = NULL;

        // Find field by character
        if(c < 1000) {
            field = find_field_by_short(fields, c);
        } else {
            // Find by index
            int field_idx = c - 1000;
            if(field_idx >= 0 && field_idx < field_count) { field = &fields[field_idx]; }
        }

        if(!field) {
            error("Unknown option");
            result = -1;
            break;
        }

        // Set value
        const char *value = optarg;
        if(field->type == PTK_CONFIG_BOOL) {
            value = "true";  // Boolean flags are true when present
        }

        if(set_field_value(field, value) != 0) {
            result = -1;
            break;
        }
    }

    free(long_options);
    return result;
}
