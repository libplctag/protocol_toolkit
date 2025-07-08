#ifndef TEST_SIMPLE_FIXED_TEST_SIMPLE_FIXED_H
#define TEST_SIMPLE_FIXED_TEST_SIMPLE_FIXED_H

#include "ptk_array.h"
#include "ptk_allocator.h"
#include "ptk_buf.h"
#include "ptk_codec.h"
#include "ptk_err.h"
#include "ptk_log.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Message Type Enumeration */
typedef enum {
    SIMPLE_MESSAGE_MESSAGE_TYPE = 1,
} message_type_t;

/* Constants */
#define TEST_CONSTANT ((u16_le_t)4660)

/* simple_message message definition */
typedef struct simple_message_t {
    const int message_type;
} simple_message_t;

/* Constructor/Destructor */
ptk_err simple_message_create(ptk_allocator_t *alloc, simple_message_t **instance);
void simple_message_dispose(ptk_allocator_t *alloc, simple_message_t *instance);

/* Encode/Decode */
ptk_err simple_message_encode(ptk_allocator_t *alloc, ptk_buf_t *buf, const simple_message_t *instance);
ptk_err simple_message_decode(ptk_allocator_t *alloc, simple_message_t **instance, ptk_buf_t *buf);

#ifdef __cplusplus
}
#endif

#endif /* TEST_SIMPLE_FIXED_TEST_SIMPLE_FIXED_H */
