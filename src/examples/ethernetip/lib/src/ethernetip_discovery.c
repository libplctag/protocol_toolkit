#include "../include/ethernetip.h"
#include <ptk_mem.h>
#include <ptk_utils.h>
#include <unistd.h>

//=============================================================================
// SIMPLE DISCOVERY CONVENIENCE FUNCTION
//=============================================================================

int eip_discover_devices_simple(ptk_duration_ms response_time_range_ms, 
                               eip_device_found_callback_t callback, 
                               void *user_data) {
    // Validate response time range (EtherNet/IP spec: max 2000ms)
    if (response_time_range_ms <= 0 || response_time_range_ms > 2000) {
        response_time_range_ms = 500; // Default 500ms
    }
    
    // Get network interfaces from discovery API
    ptk_network_interface_array_t *interfaces = ptk_network_list_interfaces();
    if (!interfaces) {
        return -1; // No network interfaces available
    }
    
    size_t interface_count = ptk_network_interface_array_len(interfaces);
    if (interface_count == 0) {
        ptk_free(&interfaces);
        return -1;
    }
    
    int total_devices_found = 0;
    
    // Send broadcast to each suitable network interface
    for (size_t i = 0; i < interface_count; i++) {
        const ptk_network_interface_t *iface = ptk_network_interface_array_get(interfaces, i);
        
        // Skip interfaces that can't be used for discovery
        if (!iface || !iface->is_up || iface->is_loopback || !iface->supports_broadcast) {
            continue;
        }
        
        const char *broadcast_addr = iface->broadcast;
        
        // Create UDP connection for this broadcast address
        eip_connection_t *conn = eip_client_connect_udp(broadcast_addr, 44818);
        if (!conn) {
            continue; // Skip this interface
        }
        
        // Create ListIdentity request with proper response time range
        eip_pdu_base_t *request = eip_pdu_create_from_type(conn, EIP_LIST_IDENTITY_REQ_TYPE);
        if (!request) {
            ptk_free(&conn);
            continue;
        }
        
        // Set the response time range in the request
        eip_list_identity_req_t *req = (eip_list_identity_req_t *)request;
        req->response_time_range_ms = (uint16_t)response_time_range_ms;
        
        // Send ONE broadcast
        eip_pdu_send(&request, 0);
        
        // Wait for ALL responses during the full response time range
        // Add small buffer (100ms) to account for network delays
        ptk_time_ms collection_timeout = response_time_range_ms + 100;
        ptk_time_ms start_time = ptk_now_ms();
        ptk_time_ms end_time = start_time + collection_timeout;
        
        int devices_on_this_network = 0;
        
        // Continuously receive responses until timeout
        while (ptk_now_ms() < end_time) {
            ptk_time_ms remaining = end_time - ptk_now_ms();
            if (remaining <= 0) break;
            
            // Use remaining time as timeout, but cap at 500ms per receive
            ptk_duration_ms recv_timeout = (remaining > 500) ? 500 : remaining;
            
            eip_pdu_u pdu = eip_pdu_recv(conn, recv_timeout);
            
            if (pdu.base && pdu.base->pdu_type == EIP_LIST_IDENTITY_RESP_TYPE) {
                devices_on_this_network++;
                total_devices_found++;
                
                // Call user callback if provided
                if (callback) {
                    callback(pdu.list_identity_resp, user_data);
                }
                
                ptk_free(&pdu.base);
            }
            // Continue receiving even on timeout - more responses may come
        }
        
        ptk_free(&conn);
    }
    
    ptk_free(&interfaces);
    return total_devices_found;
}