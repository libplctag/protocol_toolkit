#ifndef CIP_PROTOCOL_H
#define CIP_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// CIP PDU Types
typedef enum {
    CIP_PDU_TYPE_NONE = 0,
    CIP_PDU_TYPE_READ,
    CIP_PDU_TYPE_WRITE,
    CIP_PDU_TYPE_CUSTOM,
    // Add more as needed
} CipPduType;

// CIP PDU Structs
typedef struct {
    uint8_t service;
    uint8_t class_id;
    uint8_t instance_id;
} CipPduRead;

typedef struct {
    uint8_t service;
    uint8_t class_id;
    uint8_t instance_id;
    uint8_t data[256];
    size_t data_len;
} CipPduWrite;

// Discriminated Union
typedef struct {
    CipPduType type;
    union {
        CipPduRead* read;
        CipPduWrite* write;
        void* raw;
    };
} CipPdu;

// Parser Function Type
typedef int (*CipParserFn)(const uint8_t* data,
                           size_t len,
                           CipPdu* out_pdu,
                           void* context);

// Public API
int get_cip_pdu(int sock,
                uint8_t* buffer,
                size_t buffer_len,
                CipPdu* out_pdu,
                void* context);

int cip_send_pdu_read(int sock,
                      uint8_t* buffer,
                      size_t buffer_len,
                      const CipPduRead* pdu,
                      void* context);

int cip_send_pdu_write(int sock,
                       uint8_t* buffer,
                       size_t buffer_len,
                       const CipPduWrite* pdu,
                       void* context);

// Registration
void cip_register_parsers(void);

#ifdef __cplusplus
}
#endif

#endif // CIP_PROTOCOL_H
