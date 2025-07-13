/**
 * @file address.c
 * @brief Linux implementation of address-related functions for Protocol Toolkit
 */
#include <ptk_sock.h>
#include <ptk_alloc.h>
#include <ptk_err.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdbool.h>

/**
 * @brief Check if two addresses are equal
 */
bool ptk_address_equals(const ptk_address_t *addr1, const ptk_address_t *addr2) {
    if (!addr1 || !addr2) return false;
    return addr1->ip == addr2->ip && addr1->port == addr2->port && addr1->family == addr2->family;
}

/**
 * @brief Get the port number from an address structure
 */
uint16_t ptk_address_get_port(const ptk_address_t *address) {
    if (!address) return 0;
    return address->port;
}

/**
 * @brief Initialize an address structure from IP string and port
 */
ptk_err ptk_address_init(ptk_address_t *address, const char *ip_string, uint16_t port) {
    if (!address) return PTK_ERR_INVALID_PARAM;
    address->port = port;
    address->family = AF_INET;
    address->reserved = 0;
    if (!ip_string || strcmp(ip_string, "0.0.0.0") == 0) {
        address->ip = htonl(INADDR_ANY);
        return PTK_OK;
    }
    struct in_addr addr;
    if (inet_aton(ip_string, &addr) == 0) {
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return PTK_ERR_INVALID_PARAM;
    }
    address->ip = addr.s_addr;
    return PTK_OK;
}

/**
 * @brief Initialize an address for any interface (INADDR_ANY)
 */
ptk_err ptk_address_init_any(ptk_address_t *address, uint16_t port) {
    if (!address) return PTK_ERR_INVALID_PARAM;
    address->ip = htonl(INADDR_ANY);
    address->port = port;
    address->family = AF_INET;
    address->reserved = 0;
    return PTK_OK;
}

/**
 * @brief Convert an address structure to an IP string
 */
char *ptk_address_to_string(const ptk_address_t *address) {
    if (!address) return NULL;
    struct in_addr addr;
    addr.s_addr = address->ip;
    char buf[16];
    if (!inet_ntop(AF_INET, &addr, buf, sizeof(buf))) {
        ptk_set_err(PTK_ERR_INVALID_PARAM);
        return NULL;
    }
    char *result = ptk_alloc(strlen(buf) + 1, NULL);
    if (!result) {
        ptk_set_err(PTK_ERR_NO_RESOURCES);
        return NULL;
    }
    strcpy(result, buf);
    return result;
}
