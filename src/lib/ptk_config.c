#include "ptk_config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static void set_field_value(const ptk_config_field_t *field, const char *value) {
    switch (field->type) {
        case PTK_CONFIG_STRING:
            *(char **)field->target = strdup(value);
            break;
        case PTK_CONFIG_INT:
            *(int *)field->target = atoi(value);
            break;
        case PTK_CONFIG_UINT16:
            *(uint16_t *)field->target = (uint16_t)strtoul(value, NULL, 0);
            break;
        case PTK_CONFIG_BOOL:
            if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0)
                *(bool *)field->target = true;
            else
                *(bool *)field->target = false;
            break;
        default:
            break;
    }
}

ptk_err ptk_config_parse(int argc, char *argv[], const ptk_config_field_t *fields, const char *program_name) {
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            ptk_config_print_help(program_name ? program_name : argv[0], fields, NULL);
            return PTK_OK;
        }
        for (const ptk_config_field_t *f = fields; f->name; ++f) {
            char long_opt[64];
            snprintf(long_opt, sizeof(long_opt), "--%s", f->name);
            char short_opt[4] = "";
            if (f->short_name) {
                snprintf(short_opt, sizeof(short_opt), "-%c", f->short_name);
            }
            if ((f->name && strcmp(arg, long_opt) == 0) || (f->short_name && strcmp(arg, short_opt) == 0)) {
                if (f->type == PTK_CONFIG_HELP) {
                    ptk_config_print_help(program_name ? program_name : argv[0], fields, NULL);
                    return PTK_OK;
                }
                if (i + 1 < argc) {
                    set_field_value(f, argv[++i]);
                } else {
                    fprintf(stderr, "Missing value for option %s\n", arg);
                    return PTK_ERR_INVALID_PARAM;
                }
                break;
            }
        }
    }
    // Set defaults for fields not set
    for (const ptk_config_field_t *f = fields; f->name; ++f) {
        if (f->default_str && f->target) {
            set_field_value(f, f->default_str);
        }
    }
    return PTK_OK;
}

ptk_err ptk_config_print_help(const char *program_name, const ptk_config_field_t *fields, const char *description) {
    printf("Usage: %s [options]\n", program_name);
    if (description) printf("%s\n", description);
    printf("Options:\n");
    for (const ptk_config_field_t *f = fields; f->name; ++f) {
        printf("  --%s", f->name);
        if (f->short_name) printf(", -%c", f->short_name);
        printf("\t%s", f->help ? f->help : "");
        if (f->default_str) printf(" (default: %s)", f->default_str);
        printf("\n");
    }
    printf("  --help, -h\tShow this help message\n");
    return PTK_OK;
}
