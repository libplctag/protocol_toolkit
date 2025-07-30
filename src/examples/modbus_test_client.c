#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>

#define MODBUS_TCP_PORT 5020
#define MODBUS_TCP_HEADER_SIZE 6

// Utility functions
static void write_uint16_be(uint8_t *data, uint16_t value) {
    data[0] = (value >> 8) & 0xFF;
    data[1] = value & 0xFF;
}

static uint16_t read_uint16_be(const uint8_t *data) { return (data[0] << 8) | data[1]; }

// Send a Modbus TCP request and receive response
static int modbus_request(int sock, uint16_t transaction_id, uint8_t unit_id, const uint8_t *pdu, size_t pdu_len,
                          uint8_t *response, size_t *response_len) {
    uint8_t frame[256];

    // Build Modbus TCP header
    write_uint16_be(&frame[0], transaction_id);  // Transaction ID
    write_uint16_be(&frame[2], 0);               // Protocol ID (0 = Modbus)
    write_uint16_be(&frame[4], pdu_len + 1);     // Length (PDU + unit ID)
    frame[6] = unit_id;                          // Unit ID

    // Copy PDU
    memcpy(&frame[7], pdu, pdu_len);

    // Send request
    size_t frame_len = MODBUS_TCP_HEADER_SIZE + 1 + pdu_len;
    ssize_t sent = send(sock, frame, frame_len, 0);
    if(sent != (ssize_t)frame_len) {
        printf("Failed to send complete request\n");
        return -1;
    }

    // Receive response
    ssize_t received = recv(sock, frame, sizeof(frame), 0);
    if(received < MODBUS_TCP_HEADER_SIZE + 1) {
        printf("Received incomplete response (%zd bytes)\n", received);
        return -1;
    }

    // Parse header
    uint16_t resp_transaction = read_uint16_be(&frame[0]);
    uint16_t resp_protocol = read_uint16_be(&frame[2]);
    uint16_t resp_length = read_uint16_be(&frame[4]);
    uint8_t resp_unit = frame[6];

    printf("Response: TID=%d, Protocol=%d, Length=%d, Unit=%d\n", resp_transaction, resp_protocol, resp_length, resp_unit);

    if(resp_transaction != transaction_id || resp_protocol != 0 || resp_unit != unit_id) {
        printf("Response header mismatch\n");
        return -1;
    }

    // Copy response PDU
    size_t resp_pdu_len = resp_length - 1;  // Subtract unit ID
    memcpy(response, &frame[7], resp_pdu_len);
    *response_len = resp_pdu_len;

    return 0;
}

int main(void) {
    printf("Modbus TCP Test Client\n");
    printf("Connecting to localhost:%d\n", MODBUS_TCP_PORT);

    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0) {
        perror("socket");
        return 1;
    }

    // Connect to server
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(MODBUS_TCP_PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if(connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }

    printf("Connected to Modbus TCP server\n\n");

    uint8_t pdu[256];
    uint8_t response[256];
    size_t response_len;
    uint16_t transaction_id = 1;

    // Test 1: Read Coils (function 0x01)
    printf("=== Test 1: Read Coils (0x01) ===\n");
    pdu[0] = 0x01;                 // Function code: Read Coils
    write_uint16_be(&pdu[1], 0);   // Starting address: 0
    write_uint16_be(&pdu[3], 10);  // Quantity: 10 coils

    if(modbus_request(sock, transaction_id++, 1, pdu, 5, response, &response_len) == 0) {
        printf("Function: 0x%02X, Byte count: %d\n", response[0], response[1]);
        printf("Coil values: ");
        for(int bit = 0; bit < 10; bit++) {
            int byte_idx = bit / 8;
            int bit_idx = bit % 8;
            bool coil_value = (response[2 + byte_idx] >> bit_idx) & 1;
            printf("%d ", coil_value ? 1 : 0);
        }
        printf("\n\n");
    }

    // Test 2: Read Holding Registers (function 0x03)
    printf("=== Test 2: Read Holding Registers (0x03) ===\n");
    pdu[0] = 0x03;                // Function code: Read Holding Registers
    write_uint16_be(&pdu[1], 0);  // Starting address: 0
    write_uint16_be(&pdu[3], 5);  // Quantity: 5 registers

    if(modbus_request(sock, transaction_id++, 1, pdu, 5, response, &response_len) == 0) {
        printf("Function: 0x%02X, Byte count: %d\n", response[0], response[1]);
        printf("Register values: ");
        for(int i = 0; i < 5; i++) {
            uint16_t reg_value = read_uint16_be(&response[2 + i * 2]);
            printf("%d ", reg_value);
        }
        printf("\n\n");
    }

    // Test 3: Write Single Coil (function 0x05)
    printf("=== Test 3: Write Single Coil (0x05) ===\n");
    pdu[0] = 0x05;                     // Function code: Write Single Coil
    write_uint16_be(&pdu[1], 10);      // Coil address: 10
    write_uint16_be(&pdu[3], 0xFF00);  // Value: ON (0xFF00)

    if(modbus_request(sock, transaction_id++, 1, pdu, 5, response, &response_len) == 0) {
        printf("Function: 0x%02X, Address: %d, Value: 0x%04X\n", response[0], read_uint16_be(&response[1]),
               read_uint16_be(&response[3]));
        printf("Coil 10 set to ON\n\n");
    }

    // Test 4: Write Single Register (function 0x06)
    printf("=== Test 4: Write Single Register (0x06) ===\n");
    pdu[0] = 0x06;                   // Function code: Write Single Register
    write_uint16_be(&pdu[1], 5);     // Register address: 5
    write_uint16_be(&pdu[3], 9999);  // Value: 9999

    if(modbus_request(sock, transaction_id++, 1, pdu, 5, response, &response_len) == 0) {
        printf("Function: 0x%02X, Address: %d, Value: %d\n", response[0], read_uint16_be(&response[1]),
               read_uint16_be(&response[3]));
        printf("Register 5 set to 9999\n\n");
    }

    // Test 5: Write Multiple Registers (function 0x10)
    printf("=== Test 5: Write Multiple Registers (0x10) ===\n");
    pdu[0] = 0x10;                    // Function code: Write Multiple Registers
    write_uint16_be(&pdu[1], 10);     // Starting address: 10
    write_uint16_be(&pdu[3], 3);      // Quantity: 3 registers
    pdu[5] = 6;                       // Byte count: 6 (3 registers * 2 bytes)
    write_uint16_be(&pdu[6], 1111);   // Register 10 = 1111
    write_uint16_be(&pdu[8], 2222);   // Register 11 = 2222
    write_uint16_be(&pdu[10], 3333);  // Register 12 = 3333

    if(modbus_request(sock, transaction_id++, 1, pdu, 12, response, &response_len) == 0) {
        printf("Function: 0x%02X, Starting address: %d, Quantity: %d\n", response[0], read_uint16_be(&response[1]),
               read_uint16_be(&response[3]));
        printf("Registers 10-12 written\n\n");
    }

    // Test 6: Read back the written registers
    printf("=== Test 6: Read back written registers ===\n");
    pdu[0] = 0x03;                // Function code: Read Holding Registers
    write_uint16_be(&pdu[1], 5);  // Starting address: 5 (includes register we wrote)
    write_uint16_be(&pdu[3], 8);  // Quantity: 8 registers (5-12)

    if(modbus_request(sock, transaction_id++, 1, pdu, 5, response, &response_len) == 0) {
        printf("Function: 0x%02X, Byte count: %d\n", response[0], response[1]);
        printf("Register values (5-12): ");
        for(int i = 0; i < 8; i++) {
            uint16_t reg_value = read_uint16_be(&response[2 + i * 2]);
            printf("[%d]=%d ", 5 + i, reg_value);
        }
        printf("\n\n");
    }

    // Test 7: Read back coil 10
    printf("=== Test 7: Read back coil 10 ===\n");
    pdu[0] = 0x01;                 // Function code: Read Coils
    write_uint16_be(&pdu[1], 10);  // Starting address: 10
    write_uint16_be(&pdu[3], 1);   // Quantity: 1 coil

    if(modbus_request(sock, transaction_id++, 1, pdu, 5, response, &response_len) == 0) {
        printf("Function: 0x%02X, Byte count: %d\n", response[0], response[1]);
        bool coil_10 = response[2] & 1;
        printf("Coil 10 value: %s\n\n", coil_10 ? "ON" : "OFF");
    }

    // Test 8: Error test - try to read beyond register range
    printf("=== Test 8: Error test - read beyond range ===\n");
    pdu[0] = 0x03;                 // Function code: Read Holding Registers
    write_uint16_be(&pdu[1], 99);  // Starting address: 99 (last valid register)
    write_uint16_be(&pdu[3], 5);   // Quantity: 5 (would go beyond 100 registers)

    if(modbus_request(sock, transaction_id++, 1, pdu, 5, response, &response_len) == 0) {
        if(response[0] & 0x80) {
            printf("Exception response: Function 0x%02X, Exception code: %d\n", response[0] & 0x7F, response[1]);
        } else {
            printf("Unexpected success response\n");
        }
    }

    printf("\n=== Test completed ===\n");

    close(sock);
    return 0;
}
