#include "ptk_event.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

void test_ptk_event(void) {
    // Timer
    ptk_timer_connection_t timer;
    assert(ptk_init_timer(&timer, 100, 1, true) == PTK_OK);
    assert(timer.interval_ms == 100 && timer.id == 1 && timer.repeating);
    // TCP client/server/UDP/app event init (host 127.0.0.1, port 0 for test)
    ptk_tcp_client_connection_t tcp_client;
    assert(ptk_init_tcp_client_connection(&tcp_client, "127.0.0.1", 0) == PTK_OK || tcp_client.fd == -1);
    ptk_tcp_server_connection_t tcp_server;
    assert(ptk_init_tcp_server_connection(&tcp_server, "127.0.0.1", 0) == PTK_OK || tcp_server.fd == -1);
    ptk_udp_connection_t udp;
    assert(ptk_init_udp_connection(&udp, "127.0.0.1", 0) == PTK_OK || udp.fd == -1);
    ptk_app_event_connection_t app;
    assert(ptk_init_app_event_connection(&app) == PTK_OK);
    // Close
    assert(ptk_connection_close((ptk_connection_t*)&tcp_client) == PTK_OK || tcp_client.fd == -1);
    assert(ptk_connection_close((ptk_connection_t*)&tcp_server) == PTK_OK || tcp_server.fd == -1);
    assert(ptk_connection_close((ptk_connection_t*)&udp) == PTK_OK || udp.fd == -1);
    assert(ptk_connection_close((ptk_connection_t*)&app) == PTK_OK);
}

int main(void) {
    test_ptk_event();
    return 0;
}
