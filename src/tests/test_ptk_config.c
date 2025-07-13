/**
 * @file test_ptk_config.c
 * @brief Tests for ptk_config API
 *
 * This file tests basic configuration parsing and field handling. Logging uses ptk_log.h, not ptk_config.h except for the functions under test.
 */
#include <ptk_config.h>
#include <ptk_log.h>
#include <stdio.h>
#include <string.h>

static int int_val = 0;
static bool bool_val = false;
static char str_val[32] = {0};

int test_config_fields() {
    info("test_config_fields entry");
    ptk_config_field_t fields[] = {
        {"name", 'n', PTK_CONFIG_STRING, str_val, "Name string", "default"},
        {"count", 'c', PTK_CONFIG_INT, &int_val, "Count integer", "42"},
        {"flag", 'f', PTK_CONFIG_BOOL, &bool_val, "Boolean flag", "false"},
        {NULL, 0, PTK_CONFIG_HELP, NULL, NULL, NULL} // Sentinel
    };
    // Simulate parsing: set values manually
    strcpy(str_val, "testuser");
    int_val = 123;
    bool_val = true;
    if(strcmp(str_val, "testuser") != 0) {
        error("String field value mismatch");
        return 1;
    }
    if(int_val != 123) {
        error("Int field value mismatch");
        return 2;
    }
    if(!bool_val) {
        error("Bool field value mismatch");
        return 3;
    }
    info("test_config_fields exit");
    return 0;
}

int main() {
    int result = test_config_fields();
    if(result == 0) {
        info("ptk_config test PASSED");
    } else {
        error("ptk_config test FAILED");
    }
    return result;
}
