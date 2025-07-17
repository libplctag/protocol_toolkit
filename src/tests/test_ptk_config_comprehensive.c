/**
 * @file test_ptk_config_comprehensive.c
 * @brief Comprehensive tests for ptk_config.h API
 *
 * Tests all configuration parsing functionality including argument parsing
 * and help generation.
 */

#include <ptk_config.h>
#include <ptk_log.h>
#include <ptk_err.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

//=============================================================================
// Basic Configuration Parsing Tests
//=============================================================================

int test_config_basic_parsing(void) {
    info("test_config_basic_parsing entry");
    
    // Set up configuration fields
    char *string_val = NULL;
    int int_val = 0;
    uint16_t uint16_val = 0;
    bool bool_val = false;
    
    const ptk_config_field_t fields[] = {
        {"string", 's', PTK_CONFIG_STRING, &string_val, "String value", "default"},
        {"integer", 'i', PTK_CONFIG_INT, &int_val, "Integer value", "0"},
        {"port", 'p', PTK_CONFIG_UINT16, &uint16_val, "Port number", "8080"},
        {"verbose", 'v', PTK_CONFIG_BOOL, &bool_val, "Verbose output", "false"},
        PTK_CONFIG_END
    };
    
    // Test basic string parsing
    char *test_argv1[] = {"test_prog", "--string", "hello"};
    ptk_err_t result = ptk_config_parse(3, test_argv1, fields, "test_prog");
    if (result != PTK_OK) {
        error("String parsing failed");
        return 1;
    }
    
    if (!string_val || strcmp(string_val, "hello") != 0) {
        error("String value not set correctly: %s", string_val ? string_val : "NULL");
        return 2;
    }
    
    // Test short option
    int_val = 0;  // Reset
    char *test_argv2[] = {"test_prog", "-i", "42"};
    result = ptk_config_parse(3, test_argv2, fields, "test_prog");
    if (result != PTK_OK) {
        error("Short option parsing failed");
        return 3;
    }
    
    if (int_val != 42) {
        error("Integer value not set correctly: %d", int_val);
        return 4;
    }
    
    // Test uint16
    uint16_val = 0;  // Reset
    char *test_argv3[] = {"test_prog", "--port", "3000"};
    result = ptk_config_parse(3, test_argv3, fields, "test_prog");
    if (result != PTK_OK) {
        error("uint16 parsing failed");
        return 5;
    }
    
    if (uint16_val != 3000) {
        error("uint16 value not set correctly: %u", uint16_val);
        return 6;
    }
    
    // Test boolean flag (presence means true)
    bool_val = false;  // Reset
    char *test_argv4[] = {"test_prog", "-v"};
    result = ptk_config_parse(2, test_argv4, fields, "test_prog");
    if (result != PTK_OK) {
        error("Boolean flag parsing failed");
        return 7;
    }
    
    if (!bool_val) {
        error("Boolean flag not set correctly");
        return 8;
    }
    
    info("test_config_basic_parsing exit");
    return 0;
}

int test_config_multiple_arguments(void) {
    info("test_config_multiple_arguments entry");
    
    char *string_val = NULL;
    int int_val = 0;
    bool verbose = false;
    
    const ptk_config_field_t fields[] = {
        {"name", 'n', PTK_CONFIG_STRING, &string_val, "Name", "anonymous"},
        {"count", 'c', PTK_CONFIG_INT, &int_val, "Count", "1"},
        {"verbose", 'v', PTK_CONFIG_BOOL, &verbose, "Verbose", "false"},
        PTK_CONFIG_END
    };
    
    // Test multiple arguments at once
    char *test_argv[] = {"test_prog", "-n", "testname", "-c", "100", "-v"};
    ptk_err_t result = ptk_config_parse(6, test_argv, fields, "test_prog");
    if (result != PTK_OK) {
        error("Multiple argument parsing failed");
        return 1;
    }
    
    if (!string_val || strcmp(string_val, "testname") != 0) {
        error("String value incorrect: %s", string_val ? string_val : "NULL");
        return 2;
    }
    
    if (int_val != 100) {
        error("Integer value incorrect: %d", int_val);
        return 3;
    }
    
    if (!verbose) {
        error("Boolean flag not set");
        return 4;
    }
    
    // Test mixed long and short options
    string_val = NULL;
    int_val = 0;
    verbose = false;
    
    char *test_argv2[] = {"test_prog", "--name", "longname", "-c", "200", "--verbose"};
    result = ptk_config_parse(6, test_argv2, fields, "test_prog");
    if (result != PTK_OK) {
        error("Mixed option parsing failed");
        return 5;
    }
    
    if (!string_val || strcmp(string_val, "longname") != 0) {
        error("Long option string value incorrect: %s", string_val ? string_val : "NULL");
        return 6;
    }
    
    if (int_val != 200) {
        error("Mixed option integer value incorrect: %d", int_val);
        return 7;
    }
    
    if (!verbose) {
        error("Long boolean flag not set");
        return 8;
    }
    
    info("test_config_multiple_arguments exit");
    return 0;
}

int test_config_edge_cases(void) {
    info("test_config_edge_cases entry");
    
    int int_val = 0;
    uint16_t port_val = 0;
    
    const ptk_config_field_t fields[] = {
        {"number", 'n', PTK_CONFIG_INT, &int_val, "Number", "0"},
        {"port", 'p', PTK_CONFIG_UINT16, &port_val, "Port", "8080"},
        PTK_CONFIG_END
    };
    
    // Test negative number
    char *test_argv1[] = {"test_prog", "-n", "-123"};
    ptk_err_t result = ptk_config_parse(3, test_argv1, fields, "test_prog");
    if (result != PTK_OK) {
        error("Negative number parsing failed");
        return 1;
    }
    
    if (int_val != -123) {
        error("Negative number not parsed correctly: %d", int_val);
        return 2;
    }
    
    // Test maximum uint16 value
    port_val = 0;  // Reset
    char *test_argv2[] = {"test_prog", "--port", "65535"};
    result = ptk_config_parse(3, test_argv2, fields, "test_prog");
    if (result != PTK_OK) {
        error("Maximum uint16 parsing failed");
        return 3;
    }
    
    if (port_val != 65535) {
        error("Maximum uint16 not parsed correctly: %u", port_val);
        return 4;
    }
    
    // Test zero values
    int_val = 999;  // Set to non-zero
    port_val = 999;
    char *test_argv3[] = {"test_prog", "-n", "0", "-p", "0"};
    result = ptk_config_parse(5, test_argv3, fields, "test_prog");
    if (result != PTK_OK) {
        error("Zero value parsing failed");
        return 5;
    }
    
    if (int_val != 0) {
        error("Zero int not parsed correctly: %d", int_val);
        return 6;
    }
    
    if (port_val != 0) {
        error("Zero uint16 not parsed correctly: %u", port_val);
        return 7;
    }
    
    info("test_config_edge_cases exit");
    return 0;
}

int test_config_error_conditions(void) {
    info("test_config_error_conditions entry");
    
    int int_val = 0;
    uint16_t port_val = 0;
    
    const ptk_config_field_t fields[] = {
        {"number", 'n', PTK_CONFIG_INT, &int_val, "Number", "0"},
        {"port", 'p', PTK_CONFIG_UINT16, &port_val, "Port", "8080"},
        PTK_CONFIG_END
    };
    
    // Test unknown option
    char *test_argv1[] = {"test_prog", "--unknown"};
    ptk_err_t result = ptk_config_parse(2, test_argv1, fields, "test_prog");
    if (result == PTK_OK) {
        error("Unknown option should have failed");
        return 1;
    }
    
    // Test missing value for option that requires one
    char *test_argv2[] = {"test_prog", "-n"};
    result = ptk_config_parse(2, test_argv2, fields, "test_prog");
    if (result == PTK_OK) {
        error("Missing value should have failed");
        return 2;
    }
    
    // Test invalid integer
    char *test_argv3[] = {"test_prog", "-n", "not_a_number"};
    result = ptk_config_parse(3, test_argv3, fields, "test_prog");
    if (result == PTK_OK) {
        error("Invalid integer should have failed");
        return 3;
    }
    
    // Test uint16 overflow
    char *test_argv4[] = {"test_prog", "-p", "100000"};
    result = ptk_config_parse(3, test_argv4, fields, "test_prog");
    if (result == PTK_OK) {
        error("uint16 overflow should have failed");
        return 4;
    }
    
    // Test negative uint16
    char *test_argv5[] = {"test_prog", "-p", "-1"};
    result = ptk_config_parse(3, test_argv5, fields, "test_prog");
    if (result == PTK_OK) {
        error("Negative uint16 should have failed");
        return 5;
    }
    
    info("test_config_error_conditions exit");
    return 0;
}

//=============================================================================
// Help Generation Tests
//=============================================================================

int test_config_help_generation(void) {
    info("test_config_help_generation entry");
    
    char *string_val = NULL;
    int int_val = 0;
    bool verbose = false;
    
    const ptk_config_field_t fields[] = {
        {"name", 'n', PTK_CONFIG_STRING, &string_val, "Specify the name", "default"},
        {"count", 'c', PTK_CONFIG_INT, &int_val, "Number of iterations", "10"},
        {"verbose", 'v', PTK_CONFIG_BOOL, &verbose, "Enable verbose output", "false"},
        {"help", 'h', PTK_CONFIG_HELP, NULL, "Show this help message", NULL},
        PTK_CONFIG_END
    };
    
    // Test help generation function
    ptk_err_t result = ptk_config_print_help("test_program", fields, "A test program for configuration parsing");
    if (result != PTK_OK) {
        error("Help generation failed");
        return 1;
    }
    
    // Test help flag parsing (should return 1 to indicate help was shown)
    char *test_argv[] = {"test_prog", "--help"};
    result = ptk_config_parse(2, test_argv, fields, "test_prog");
    if (result != 1) {
        error("Help flag should return 1, got %d", result);
        return 2;
    }
    
    // Test short help flag
    char *test_argv2[] = {"test_prog", "-h"};
    result = ptk_config_parse(2, test_argv2, fields, "test_prog");
    if (result != 1) {
        error("Short help flag should return 1, got %d", result);
        return 3;
    }
    
    info("test_config_help_generation exit");
    return 0;
}

int test_config_program_name_variations(void) {
    info("test_config_program_name_variations entry");
    
    bool verbose = false;
    
    const ptk_config_field_t fields[] = {
        {"verbose", 'v', PTK_CONFIG_BOOL, &verbose, "Verbose output", "false"},
        PTK_CONFIG_END
    };
    
    // Test with NULL program name (should use argv[0])
    char *test_argv1[] = {"my_program", "-v"};
    ptk_err_t result = ptk_config_parse(2, test_argv1, fields, NULL);
    if (result != PTK_OK) {
        error("Parse with NULL program name failed");
        return 1;
    }
    
    if (!verbose) {
        error("Verbose flag not set with NULL program name");
        return 2;
    }
    
    // Test with explicit program name
    verbose = false;  // Reset
    result = ptk_config_parse(2, test_argv1, fields, "explicit_name");
    if (result != PTK_OK) {
        error("Parse with explicit program name failed");
        return 3;
    }
    
    if (!verbose) {
        error("Verbose flag not set with explicit program name");
        return 4;
    }
    
    // Test help with different program names
    result = ptk_config_print_help("custom_program", fields, NULL);
    if (result != PTK_OK) {
        error("Help with custom program name failed");
        return 5;
    }
    
    result = ptk_config_print_help(NULL, fields, "Program with NULL name");
    if (result != PTK_OK) {
        error("Help with NULL program name failed");
        return 6;
    }
    
    info("test_config_program_name_variations exit");
    return 0;
}

//=============================================================================
// Main Test Function
//=============================================================================

int test_ptk_config_main(void) {
    info("=== Starting PTK Configuration Parsing Tests ===");
    
    int result = 0;
    
    // Basic parsing tests
    if ((result = test_config_basic_parsing()) != 0) {
        error("test_config_basic_parsing failed with code %d", result);
        return result;
    }
    
    if ((result = test_config_multiple_arguments()) != 0) {
        error("test_config_multiple_arguments failed with code %d", result);
        return result;
    }
    
    if ((result = test_config_edge_cases()) != 0) {
        error("test_config_edge_cases failed with code %d", result);
        return result;
    }
    
    if ((result = test_config_error_conditions()) != 0) {
        error("test_config_error_conditions failed with code %d", result);
        return result;
    }
    
    // Help generation tests
    if ((result = test_config_help_generation()) != 0) {
        error("test_config_help_generation failed with code %d", result);
        return result;
    }
    
    if ((result = test_config_program_name_variations()) != 0) {
        error("test_config_program_name_variations failed with code %d", result);
        return result;
    }
    
    info("=== All PTK Configuration Parsing Tests Passed ===");
    return 0;
}

int main(void) {
    return test_ptk_config_main();
}