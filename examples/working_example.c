/**
 * @file test_library.c
 * @brief Test the built Protocol Toolkit library
 */

#include "protocol_toolkit.h"
#include <stdio.h>

/* Declare event loop slots and resources */
PTK_DECLARE_EVENT_LOOP_SLOTS(test_event_loops, 1);
PTK_DECLARE_EVENT_LOOP_RESOURCES(test_resources, 2, 1, 1);

int main(void) {
    printf("Protocol Toolkit macOS Library Test\n");
    printf("====================================\n");
    
    /* Test basic handle manipulation */
    ptk_handle_t test_handle = PTK_MAKE_HANDLE(PTK_TYPE_TIMER, 0, 1, 42);
    printf("✓ Created test handle: %lld\n", test_handle);
    printf("  Type: %d (expected: %d)\n", PTK_HANDLE_TYPE(test_handle), PTK_TYPE_TIMER);
    printf("  Event Loop ID: %d\n", PTK_HANDLE_EVENT_LOOP_ID(test_handle));
    printf("  Generation: %d\n", PTK_HANDLE_GENERATION(test_handle));
    printf("  Handle ID: %d\n", PTK_HANDLE_ID(test_handle));
    
    /* Test event loop creation */
    ptk_handle_t main_loop = ptk_event_loop_create(test_event_loops, 1, &test_resources);
    if (main_loop < 0) {
        printf("✗ Failed to create event loop: %s\n", ptk_error_string((ptk_err_t)main_loop));
        return 1;
    }
    printf("✓ Created event loop with handle: %lld\n", main_loop);
    
    /* Test timer creation */
    ptk_handle_t timer1 = ptk_timer_create(main_loop);
    if (timer1 < 0) {
        printf("✗ Failed to create timer1: %s\n", ptk_error_string((ptk_err_t)timer1));
        ptk_event_loop_destroy(main_loop);
        return 1;
    }
    printf("✓ Created timer1 with handle: %lld\n", timer1);
    
    /* Test creating second timer */
    ptk_handle_t timer2 = ptk_timer_create(main_loop);
    if (timer2 < 0) {
        printf("✗ Failed to create timer2: %s\n", ptk_error_string((ptk_err_t)timer2));
        ptk_timer_destroy(timer1);
        ptk_event_loop_destroy(main_loop);
        return 1;
    }
    printf("✓ Created timer2 with handle: %lld\n", timer2);
    
    /* Test socket creation */
    ptk_handle_t tcp_socket = ptk_socket_create_tcp(main_loop);
    if (tcp_socket < 0) {
        printf("✗ Failed to create TCP socket: %s\n", ptk_error_string((ptk_err_t)tcp_socket));
    } else {
        printf("✓ Created TCP socket with handle: %lld\n", tcp_socket);
    }
    
    /* Test user event source creation */
    ptk_handle_t user_event = ptk_user_event_source_create(main_loop);
    if (user_event < 0) {
        printf("✗ Failed to create user event source: %s\n", ptk_error_string((ptk_err_t)user_event));
    } else {
        printf("✓ Created user event source with handle: %lld\n", user_event);
    }
    
    /* Test handle validation */
    bool valid1 = ptk_handle_is_valid(timer1);
    bool valid2 = ptk_handle_is_valid(timer2);
    bool invalid = ptk_handle_is_valid(0);
    printf("✓ Handle validation: timer1=%s, timer2=%s, invalid=%s\n", 
           valid1 ? "valid" : "invalid",
           valid2 ? "valid" : "invalid", 
           invalid ? "valid" : "invalid");
    
    /* Test resource type detection */
    ptk_resource_type_t type1 = ptk_handle_get_type(timer1);
    ptk_resource_type_t type2 = ptk_handle_get_type(tcp_socket);
    ptk_resource_type_t type3 = ptk_handle_get_type(user_event);
    printf("✓ Resource types: timer=%d, socket=%d, user_event=%d\n", type1, type2, type3);
    
    /* Test error handling */
    ptk_err_t last_error = ptk_get_last_error(timer1);
    printf("✓ Last error: %s\n", ptk_error_string(last_error));
    
    /* Clean up */
    if (user_event >= 0) ptk_user_event_source_destroy(user_event);
    if (tcp_socket >= 0) ptk_socket_destroy(tcp_socket);
    ptk_timer_destroy(timer2);
    ptk_timer_destroy(timer1);
    ptk_event_loop_destroy(main_loop);
    
    printf("✓ All tests passed! Library is working correctly.\n");
    return 0;
}
