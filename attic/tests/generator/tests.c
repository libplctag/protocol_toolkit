#include "tests.h"
#include <stdlib.h>
#include <string.h>

codec_err_t simple_decode(struct simple **value, buf *remaining_input_buf, buf *input_buf) {
    if(!value || !remaining_input_buf || !input_buf) { return CODEC_ERR_NULL_PTR; }

    struct simple *result = malloc(sizeof(struct simple));
    if(!result) { return CODEC_ERR_NO_MEMORY; }
    memset(result, 0, sizeof(struct simple));

    size_t offset = 0;

    info_buf("Decoding simple from input", buf_data(input_buf), buf_len(input_buf));

    {
        uint8_t bytes[4];
        for(int i = 0; i < 4; i++) {
            buf_err_t err = buf_get_u8(&bytes[i], input_buf, offset + i);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        result->foo = (bytes[2] << 24) | (bytes[3] << 16) | (bytes[1] << 8) | bytes[0];
        offset += 4;
    }

    {
        uint8_t bytes[2];
        for(int i = 0; i < 2; i++) {
            buf_err_t err = buf_get_u8(&bytes[i], input_buf, offset + i);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        result->bar = bytes[0] | (bytes[1] << 8);
        offset += 2;
    }

    // Set remaining buffer
    buf_err_t buf_err = buf_from_buf(remaining_input_buf, input_buf, offset, buf_len(input_buf) - offset);
    if(buf_err != BUF_OK) {
        simple_dispose(result);
        if(buf_err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
        if(buf_err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
    }

    *value = result;
    simple_log_info(result);
    return CODEC_OK;
}

codec_err_t simple_encode(buf *remaining_output_buf, buf *output_buf, const struct simple *value) {
    if(!remaining_output_buf || !output_buf || !value) { return CODEC_ERR_NULL_PTR; }

    size_t offset = 0;

    simple_log_info((struct simple *)value);

    {
        uint8_t bytes[4];
        bytes[2] = (value->foo >> 24) & 0xFF;
        bytes[3] = (value->foo >> 16) & 0xFF;
        bytes[1] = (value->foo >> 8) & 0xFF;
        bytes[0] = value->foo & 0xFF;
        for(int i = 0; i < 4; i++) {
            buf_err_t err = buf_set_u8(output_buf, offset + i, bytes[i]);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        offset += 4;
    }

    {
        uint8_t bytes[2];
        bytes[0] = value->bar & 0xFF;
        bytes[1] = (value->bar >> 8) & 0xFF;
        for(int i = 0; i < 2; i++) {
            buf_err_t err = buf_set_u8(output_buf, offset + i, bytes[i]);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        offset += 2;
    }

    // Set remaining buffer
    buf_err_t buf_err = buf_from_buf(remaining_output_buf, output_buf, offset, buf_len(output_buf) - offset);
    if(buf_err != BUF_OK) {
        if(buf_err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
        if(buf_err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
    }

    info_buf("Encoded simple output", buf_data(output_buf), offset);
    return CODEC_OK;
}

void simple_dispose(struct simple *value) {
    if(!value) {
        warn("Called simple_dispose with NULL pointer");
        return;
    }

    info("Disposing simple");

    free(value);
}

void simple_log_impl(const char *func, int line_num, ev_log_level log_level, struct simple *value) {
    if(!value) {
        ev_log_impl(func, line_num, log_level, "simple: NULL");
        return;
    }

    ev_log_impl(func, line_num, log_level, "foo: 0x%08X", value->foo);
    ev_log_impl(func, line_num, log_level, "bar: 0x%04X", value->bar);
}

codec_err_t embedding_simple_decode(struct embedding_simple **value, buf *remaining_input_buf, buf *input_buf) {
    if(!value || !remaining_input_buf || !input_buf) { return CODEC_ERR_NULL_PTR; }

    struct embedding_simple *result = malloc(sizeof(struct embedding_simple));
    if(!result) { return CODEC_ERR_NO_MEMORY; }
    memset(result, 0, sizeof(struct embedding_simple));

    size_t offset = 0;

    info_buf("Decoding embedding_simple from input", buf_data(input_buf), buf_len(input_buf));

    buf_err_t err = buf_get_u8((uint8_t *)&result->id, input_buf, offset);
    if(err != BUF_OK) {
        if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
        if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
    }
    offset += 1;


    {
        uint8_t bytes[8];
        for(int i = 0; i < 8; i++) {
            buf_err_t err = buf_get_u8(&bytes[i], input_buf, offset + i);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        result->float_field = (uint64_t)bytes[0] | ((uint64_t)bytes[1] << 8) | ((uint64_t)bytes[2] << 16)
                              | ((uint64_t)bytes[3] << 24) | ((uint64_t)bytes[4] << 32) | ((uint64_t)bytes[5] << 40)
                              | ((uint64_t)bytes[6] << 48) | ((uint64_t)bytes[7] << 56);
        union {
            uint64_t i;
            double f;
        } u;
        u.i = *((uint64_t *)&result->float_field);
        result->float_field = u.f;
        offset += 8;
    }

    // Set remaining buffer
    buf_err_t buf_err = buf_from_buf(remaining_input_buf, input_buf, offset, buf_len(input_buf) - offset);
    if(buf_err != BUF_OK) {
        embedding_simple_dispose(result);
        if(buf_err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
        if(buf_err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
    }

    *value = result;
    embedding_simple_log_info(result);
    return CODEC_OK;
}

codec_err_t embedding_simple_encode(buf *remaining_output_buf, buf *output_buf, const struct embedding_simple *value) {
    if(!remaining_output_buf || !output_buf || !value) { return CODEC_ERR_NULL_PTR; }

    size_t offset = 0;

    embedding_simple_log_info((struct embedding_simple *)value);

    buf_err_t err = buf_set_u8(output_buf, offset, (uint8_t)value->id);
    if(err != BUF_OK) {
        if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
        if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
    }
    offset += 1;


    {
        uint8_t bytes[8];
        union {
            uint64_t i;
            double f;
        } u;
        u.f = value->float_field;
        bytes[0] = (u.i >> 0) & 0xFF;
        bytes[1] = (u.i >> 8) & 0xFF;
        bytes[2] = (u.i >> 16) & 0xFF;
        bytes[3] = (u.i >> 24) & 0xFF;
        bytes[4] = (u.i >> 32) & 0xFF;
        bytes[5] = (u.i >> 40) & 0xFF;
        bytes[6] = (u.i >> 48) & 0xFF;
        bytes[7] = (u.i >> 56) & 0xFF;
        for(int i = 0; i < 8; i++) {
            buf_err_t err = buf_set_u8(output_buf, offset + i, bytes[i]);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        offset += 8;
    }

    // Set remaining buffer
    buf_err_t buf_err = buf_from_buf(remaining_output_buf, output_buf, offset, buf_len(output_buf) - offset);
    if(buf_err != BUF_OK) {
        if(buf_err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
        if(buf_err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
    }

    info_buf("Encoded embedding_simple output", buf_data(output_buf), offset);
    return CODEC_OK;
}

void embedding_simple_dispose(struct embedding_simple *value) {
    if(!value) {
        warn("Called embedding_simple_dispose with NULL pointer");
        return;
    }

    info("Disposing embedding_simple");

    free(value);
}

void embedding_simple_log_impl(const char *func, int line_num, ev_log_level log_level, struct embedding_simple *value) {
    if(!value) {
        ev_log_impl(func, line_num, log_level, "embedding_simple: NULL");
        return;
    }

    ev_log_impl(func, line_num, log_level, "id: 0x%02X", value->id);
    ev_log_impl(func, line_num, log_level, "simple: 0x%02X", value->simple);
    ev_log_impl(func, line_num, log_level, "float_field: %.6f", value->float_field);
}

codec_err_t array_test_decode(struct array_test **value, buf *remaining_input_buf, buf *input_buf) {
    if(!value || !remaining_input_buf || !input_buf) { return CODEC_ERR_NULL_PTR; }

    struct array_test *result = malloc(sizeof(struct array_test));
    if(!result) { return CODEC_ERR_NO_MEMORY; }
    memset(result, 0, sizeof(struct array_test));

    size_t offset = 0;

    info_buf("Decoding array_test from input", buf_data(input_buf), buf_len(input_buf));

    // Array field i64_be_array
    for(int i = 0; i < (ARRAY_LEN); i++) {
        {
            uint8_t bytes[8];
            for(int i = 0; i < 8; i++) {
                buf_err_t err = buf_get_u8(&bytes[i], input_buf, offset + i);
                if(err != BUF_OK) {
                    if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                    if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
                }
            }
            result->i64_be_array[i] = ((uint64_t)bytes[0] << 56) | ((uint64_t)bytes[1] << 48) | ((uint64_t)bytes[2] << 40)
                                      | ((uint64_t)bytes[3] << 32) | ((uint64_t)bytes[4] << 24) | ((uint64_t)bytes[5] << 16)
                                      | ((uint64_t)bytes[6] << 8) | (uint64_t)bytes[7];
            offset += 8;
        }
    }

    // Set remaining buffer
    buf_err_t buf_err = buf_from_buf(remaining_input_buf, input_buf, offset, buf_len(input_buf) - offset);
    if(buf_err != BUF_OK) {
        array_test_dispose(result);
        if(buf_err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
        if(buf_err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
    }

    *value = result;
    array_test_log_info(result);
    return CODEC_OK;
}

codec_err_t array_test_encode(buf *remaining_output_buf, buf *output_buf, const struct array_test *value) {
    if(!remaining_output_buf || !output_buf || !value) { return CODEC_ERR_NULL_PTR; }

    size_t offset = 0;

    array_test_log_info((struct array_test *)value);

    // Array field i64_be_array
    for(int i = 0; i < (ARRAY_LEN); i++) {
        {
            uint8_t bytes[8];
            bytes[0] = (value->i64_be_array[i] >> 56) & 0xFF;
            bytes[1] = (value->i64_be_array[i] >> 48) & 0xFF;
            bytes[2] = (value->i64_be_array[i] >> 40) & 0xFF;
            bytes[3] = (value->i64_be_array[i] >> 32) & 0xFF;
            bytes[4] = (value->i64_be_array[i] >> 24) & 0xFF;
            bytes[5] = (value->i64_be_array[i] >> 16) & 0xFF;
            bytes[6] = (value->i64_be_array[i] >> 8) & 0xFF;
            bytes[7] = (value->i64_be_array[i] >> 0) & 0xFF;
            for(int i = 0; i < 8; i++) {
                buf_err_t err = buf_set_u8(output_buf, offset + i, bytes[i]);
                if(err != BUF_OK) {
                    if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                    if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
                }
            }
            offset += 8;
        }
    }

    // Set remaining buffer
    buf_err_t buf_err = buf_from_buf(remaining_output_buf, output_buf, offset, buf_len(output_buf) - offset);
    if(buf_err != BUF_OK) {
        if(buf_err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
        if(buf_err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
    }

    info_buf("Encoded array_test output", buf_data(output_buf), offset);
    return CODEC_OK;
}

void array_test_dispose(struct array_test *value) {
    if(!value) {
        warn("Called array_test_dispose with NULL pointer");
        return;
    }

    info("Disposing array_test");

    free(value);
}

void array_test_log_impl(const char *func, int line_num, ev_log_level log_level, struct array_test *value) {
    if(!value) {
        ev_log_impl(func, line_num, log_level, "array_test: NULL");
        return;
    }

    for(int i = 0; i < (ARRAY_LEN); i += 16) {
        char buf[1024];
        int pos = 0;
        int end = (i + 16 < (ARRAY_LEN)) ? i + 16 : (ARRAY_LEN);
        pos += snprintf(buf + pos, sizeof(buf) - pos, "i64_be_array[%d-%d]: ", i, end - 1);
        for(int j = i; j < end; j++) {
            pos += snprintf(buf + pos, sizeof(buf) - pos, "0x%02X ", (unsigned char)value->i64_be_array[j]);
        }
        ev_log_impl(func, line_num, log_level, "%s", buf);
    }
}

codec_err_t simple_pointer_test_decode(struct simple_pointer_test **value, buf *remaining_input_buf, buf *input_buf) {
    if(!value || !remaining_input_buf || !input_buf) { return CODEC_ERR_NULL_PTR; }

    struct simple_pointer_test *result = malloc(sizeof(struct simple_pointer_test));
    if(!result) { return CODEC_ERR_NO_MEMORY; }
    memset(result, 0, sizeof(struct simple_pointer_test));

    size_t offset = 0;

    info_buf("Decoding simple_pointer_test from input", buf_data(input_buf), buf_len(input_buf));

    {
        uint8_t bytes[4];
        for(int i = 0; i < 4; i++) {
            buf_err_t err = buf_get_u8(&bytes[i], input_buf, offset + i);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        result->len = bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24);
        offset += 4;
    }

    // User-defined decode for pointer field data
    codec_err_t data_err = simple_pointer_test_data_decode(result, remaining_input_buf, input_buf);
    if(data_err != CODEC_OK) {
        simple_pointer_test_dispose(result);
        return data_err;
    }

    // Set remaining buffer
    buf_err_t buf_err = buf_from_buf(remaining_input_buf, input_buf, offset, buf_len(input_buf) - offset);
    if(buf_err != BUF_OK) {
        simple_pointer_test_dispose(result);
        if(buf_err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
        if(buf_err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
    }

    *value = result;
    simple_pointer_test_log_info(result);
    return CODEC_OK;
}

codec_err_t simple_pointer_test_encode(buf *remaining_output_buf, buf *output_buf, const struct simple_pointer_test *value) {
    if(!remaining_output_buf || !output_buf || !value) { return CODEC_ERR_NULL_PTR; }

    size_t offset = 0;

    simple_pointer_test_log_info((struct simple_pointer_test *)value);

    {
        uint8_t bytes[4];
        bytes[0] = value->len & 0xFF;
        bytes[1] = (value->len >> 8) & 0xFF;
        bytes[2] = (value->len >> 16) & 0xFF;
        bytes[3] = (value->len >> 24) & 0xFF;
        for(int i = 0; i < 4; i++) {
            buf_err_t err = buf_set_u8(output_buf, offset + i, bytes[i]);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        offset += 4;
    }

    // User-defined encode for pointer field data
    codec_err_t data_err = simple_pointer_test_data_encode(remaining_output_buf, output_buf, value);
    if(data_err != CODEC_OK) { return data_err; }

    // Set remaining buffer
    buf_err_t buf_err = buf_from_buf(remaining_output_buf, output_buf, offset, buf_len(output_buf) - offset);
    if(buf_err != BUF_OK) {
        if(buf_err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
        if(buf_err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
    }

    info_buf("Encoded simple_pointer_test output", buf_data(output_buf), offset);
    return CODEC_OK;
}

void simple_pointer_test_dispose(struct simple_pointer_test *value) {
    if(!value) {
        warn("Called simple_pointer_test_dispose with NULL pointer");
        return;
    }

    info("Disposing simple_pointer_test");

    simple_pointer_test_data_dispose(value);
    free(value);
}

void simple_pointer_test_log_impl(const char *func, int line_num, ev_log_level log_level, struct simple_pointer_test *value) {
    if(!value) {
        ev_log_impl(func, line_num, log_level, "simple_pointer_test: NULL");
        return;
    }

    ev_log_impl(func, line_num, log_level, "len: 0x%08X", value->len);
    simple_pointer_test_data_log_impl(func, line_num, log_level, value);
}

codec_err_t array_of_strings_test_decode(struct array_of_strings_test **value, buf *remaining_input_buf, buf *input_buf) {
    if(!value || !remaining_input_buf || !input_buf) { return CODEC_ERR_NULL_PTR; }

    struct array_of_strings_test *result = malloc(sizeof(struct array_of_strings_test));
    if(!result) { return CODEC_ERR_NO_MEMORY; }
    memset(result, 0, sizeof(struct array_of_strings_test));

    size_t offset = 0;

    info_buf("Decoding array_of_strings_test from input", buf_data(input_buf), buf_len(input_buf));

    {
        uint8_t bytes[2];
        for(int i = 0; i < 2; i++) {
            buf_err_t err = buf_get_u8(&bytes[i], input_buf, offset + i);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        result->num_strings = bytes[0] | (bytes[1] << 8);
        offset += 2;
    }

    // User-defined decode for pointer field array_of_c_strings
    codec_err_t array_of_c_strings_err = array_of_strings_test_array_of_c_strings_decode(result, remaining_input_buf, input_buf);
    if(array_of_c_strings_err != CODEC_OK) {
        array_of_strings_test_dispose(result);
        return array_of_c_strings_err;
    }

    // Set remaining buffer
    buf_err_t buf_err = buf_from_buf(remaining_input_buf, input_buf, offset, buf_len(input_buf) - offset);
    if(buf_err != BUF_OK) {
        array_of_strings_test_dispose(result);
        if(buf_err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
        if(buf_err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
    }

    *value = result;
    array_of_strings_test_log_info(result);
    return CODEC_OK;
}

codec_err_t array_of_strings_test_encode(buf *remaining_output_buf, buf *output_buf, const struct array_of_strings_test *value) {
    if(!remaining_output_buf || !output_buf || !value) { return CODEC_ERR_NULL_PTR; }

    size_t offset = 0;

    array_of_strings_test_log_info((struct array_of_strings_test *)value);

    {
        uint8_t bytes[2];
        bytes[0] = value->num_strings & 0xFF;
        bytes[1] = (value->num_strings >> 8) & 0xFF;
        for(int i = 0; i < 2; i++) {
            buf_err_t err = buf_set_u8(output_buf, offset + i, bytes[i]);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        offset += 2;
    }

    // User-defined encode for pointer field array_of_c_strings
    codec_err_t array_of_c_strings_err = array_of_strings_test_array_of_c_strings_encode(remaining_output_buf, output_buf, value);
    if(array_of_c_strings_err != CODEC_OK) { return array_of_c_strings_err; }

    // Set remaining buffer
    buf_err_t buf_err = buf_from_buf(remaining_output_buf, output_buf, offset, buf_len(output_buf) - offset);
    if(buf_err != BUF_OK) {
        if(buf_err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
        if(buf_err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
    }

    info_buf("Encoded array_of_strings_test output", buf_data(output_buf), offset);
    return CODEC_OK;
}

void array_of_strings_test_dispose(struct array_of_strings_test *value) {
    if(!value) {
        warn("Called array_of_strings_test_dispose with NULL pointer");
        return;
    }

    info("Disposing array_of_strings_test");

    array_of_strings_test_array_of_c_strings_dispose(value);
    free(value);
}

void array_of_strings_test_log_impl(const char *func, int line_num, ev_log_level log_level, struct array_of_strings_test *value) {
    if(!value) {
        ev_log_impl(func, line_num, log_level, "array_of_strings_test: NULL");
        return;
    }

    ev_log_impl(func, line_num, log_level, "num_strings: 0x%04X", value->num_strings);
    array_of_strings_test_array_of_c_strings_log_impl(func, line_num, log_level, value);
}

codec_err_t one_of_each_decode(struct one_of_each **value, buf *remaining_input_buf, buf *input_buf) {
    if(!value || !remaining_input_buf || !input_buf) { return CODEC_ERR_NULL_PTR; }

    struct one_of_each *result = malloc(sizeof(struct one_of_each));
    if(!result) { return CODEC_ERR_NO_MEMORY; }
    memset(result, 0, sizeof(struct one_of_each));

    size_t offset = 0;

    info_buf("Decoding one_of_each from input", buf_data(input_buf), buf_len(input_buf));

    buf_err_t err = buf_get_u8((uint8_t *)&result->field1, input_buf, offset);
    if(err != BUF_OK) {
        if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
        if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
    }
    offset += 1;

    buf_err_t err = buf_get_u8((uint8_t *)&result->field2, input_buf, offset);
    if(err != BUF_OK) {
        if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
        if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
    }
    offset += 1;

    {
        uint8_t bytes[2];
        for(int i = 0; i < 2; i++) {
            buf_err_t err = buf_get_u8(&bytes[i], input_buf, offset + i);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        result->field3 = (bytes[0] << 8) | bytes[1];
        offset += 2;
    }

    {
        uint8_t bytes[2];
        for(int i = 0; i < 2; i++) {
            buf_err_t err = buf_get_u8(&bytes[i], input_buf, offset + i);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        result->field4 = (bytes[0] << 8) | bytes[1];
        offset += 2;
    }

    {
        uint8_t bytes[2];
        for(int i = 0; i < 2; i++) {
            buf_err_t err = buf_get_u8(&bytes[i], input_buf, offset + i);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        result->field5 = bytes[0] | (bytes[1] << 8);
        offset += 2;
    }

    {
        uint8_t bytes[2];
        for(int i = 0; i < 2; i++) {
            buf_err_t err = buf_get_u8(&bytes[i], input_buf, offset + i);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        result->field6 = bytes[0] | (bytes[1] << 8);
        offset += 2;
    }

    {
        uint8_t bytes[4];
        for(int i = 0; i < 4; i++) {
            buf_err_t err = buf_get_u8(&bytes[i], input_buf, offset + i);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        result->field7 = (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
        offset += 4;
    }

    {
        uint8_t bytes[4];
        for(int i = 0; i < 4; i++) {
            buf_err_t err = buf_get_u8(&bytes[i], input_buf, offset + i);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        result->field8 = (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
        offset += 4;
    }

    {
        uint8_t bytes[4];
        for(int i = 0; i < 4; i++) {
            buf_err_t err = buf_get_u8(&bytes[i], input_buf, offset + i);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        result->field9 = (bytes[2] << 24) | (bytes[3] << 16) | (bytes[1] << 8) | bytes[0];
        offset += 4;
    }

    {
        uint8_t bytes[4];
        for(int i = 0; i < 4; i++) {
            buf_err_t err = buf_get_u8(&bytes[i], input_buf, offset + i);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        result->field10 = (bytes[2] << 24) | (bytes[3] << 16) | (bytes[1] << 8) | bytes[0];
        offset += 4;
    }

    {
        uint8_t bytes[4];
        for(int i = 0; i < 4; i++) {
            buf_err_t err = buf_get_u8(&bytes[i], input_buf, offset + i);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        result->field11 = bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24);
        offset += 4;
    }

    {
        uint8_t bytes[4];
        for(int i = 0; i < 4; i++) {
            buf_err_t err = buf_get_u8(&bytes[i], input_buf, offset + i);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        result->field12 = bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24);
        offset += 4;
    }

    {
        uint8_t bytes[4];
        for(int i = 0; i < 4; i++) {
            buf_err_t err = buf_get_u8(&bytes[i], input_buf, offset + i);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        result->field13 = bytes[1] | (bytes[0] << 8) | (bytes[3] << 16) | (bytes[2] << 24);
        offset += 4;
    }

    {
        uint8_t bytes[4];
        for(int i = 0; i < 4; i++) {
            buf_err_t err = buf_get_u8(&bytes[i], input_buf, offset + i);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        result->field15 = bytes[1] | (bytes[0] << 8) | (bytes[3] << 16) | (bytes[2] << 24);
        offset += 4;
    }

    {
        uint8_t bytes[8];
        for(int i = 0; i < 8; i++) {
            buf_err_t err = buf_get_u8(&bytes[i], input_buf, offset + i);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        result->field16 = ((uint64_t)bytes[0] << 56) | ((uint64_t)bytes[1] << 48) | ((uint64_t)bytes[2] << 40)
                          | ((uint64_t)bytes[3] << 32) | ((uint64_t)bytes[4] << 24) | ((uint64_t)bytes[5] << 16)
                          | ((uint64_t)bytes[6] << 8) | (uint64_t)bytes[7];
        offset += 8;
    }

    {
        uint8_t bytes[8];
        for(int i = 0; i < 8; i++) {
            buf_err_t err = buf_get_u8(&bytes[i], input_buf, offset + i);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        result->field17 = ((uint64_t)bytes[0] << 56) | ((uint64_t)bytes[1] << 48) | ((uint64_t)bytes[2] << 40)
                          | ((uint64_t)bytes[3] << 32) | ((uint64_t)bytes[4] << 24) | ((uint64_t)bytes[5] << 16)
                          | ((uint64_t)bytes[6] << 8) | (uint64_t)bytes[7];
        offset += 8;
    }

    {
        uint8_t bytes[8];
        for(int i = 0; i < 8; i++) {
            buf_err_t err = buf_get_u8(&bytes[i], input_buf, offset + i);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        result->field18 = ((uint64_t)bytes[6] << 56) | ((uint64_t)bytes[7] << 48) | ((uint64_t)bytes[4] << 40)
                          | ((uint64_t)bytes[5] << 32) | ((uint64_t)bytes[2] << 24) | ((uint64_t)bytes[3] << 16)
                          | ((uint64_t)bytes[0] << 8) | (uint64_t)bytes[1];
        offset += 8;
    }

    {
        uint8_t bytes[8];
        for(int i = 0; i < 8; i++) {
            buf_err_t err = buf_get_u8(&bytes[i], input_buf, offset + i);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        result->field19 = ((uint64_t)bytes[6] << 56) | ((uint64_t)bytes[7] << 48) | ((uint64_t)bytes[4] << 40)
                          | ((uint64_t)bytes[5] << 32) | ((uint64_t)bytes[2] << 24) | ((uint64_t)bytes[3] << 16)
                          | ((uint64_t)bytes[0] << 8) | (uint64_t)bytes[1];
        offset += 8;
    }

    {
        uint8_t bytes[8];
        for(int i = 0; i < 8; i++) {
            buf_err_t err = buf_get_u8(&bytes[i], input_buf, offset + i);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        result->field20 = (uint64_t)bytes[0] | ((uint64_t)bytes[1] << 8) | ((uint64_t)bytes[2] << 16) | ((uint64_t)bytes[3] << 24)
                          | ((uint64_t)bytes[4] << 32) | ((uint64_t)bytes[5] << 40) | ((uint64_t)bytes[6] << 48)
                          | ((uint64_t)bytes[7] << 56);
        offset += 8;
    }

    {
        uint8_t bytes[8];
        for(int i = 0; i < 8; i++) {
            buf_err_t err = buf_get_u8(&bytes[i], input_buf, offset + i);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        result->field21 = (uint64_t)bytes[0] | ((uint64_t)bytes[1] << 8) | ((uint64_t)bytes[2] << 16) | ((uint64_t)bytes[3] << 24)
                          | ((uint64_t)bytes[4] << 32) | ((uint64_t)bytes[5] << 40) | ((uint64_t)bytes[6] << 48)
                          | ((uint64_t)bytes[7] << 56);
        offset += 8;
    }

    {
        uint8_t bytes[8];
        for(int i = 0; i < 8; i++) {
            buf_err_t err = buf_get_u8(&bytes[i], input_buf, offset + i);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        result->field22 = (uint64_t)bytes[1] | ((uint64_t)bytes[0] << 8) | ((uint64_t)bytes[3] << 16) | ((uint64_t)bytes[2] << 24)
                          | ((uint64_t)bytes[5] << 32) | ((uint64_t)bytes[4] << 40) | ((uint64_t)bytes[7] << 48)
                          | ((uint64_t)bytes[6] << 56);
        offset += 8;
    }

    {
        uint8_t bytes[8];
        for(int i = 0; i < 8; i++) {
            buf_err_t err = buf_get_u8(&bytes[i], input_buf, offset + i);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        result->field23 = (uint64_t)bytes[1] | ((uint64_t)bytes[0] << 8) | ((uint64_t)bytes[3] << 16) | ((uint64_t)bytes[2] << 24)
                          | ((uint64_t)bytes[5] << 32) | ((uint64_t)bytes[4] << 40) | ((uint64_t)bytes[7] << 48)
                          | ((uint64_t)bytes[6] << 56);
        offset += 8;
    }

    {
        uint8_t bytes[4];
        for(int i = 0; i < 4; i++) {
            buf_err_t err = buf_get_u8(&bytes[i], input_buf, offset + i);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        result->field24 = (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
        union {
            uint32_t i;
            float f;
        } u;
        u.i = *((uint32_t *)&result->field24);
        result->field24 = u.f;
        offset += 4;
    }

    {
        uint8_t bytes[4];
        for(int i = 0; i < 4; i++) {
            buf_err_t err = buf_get_u8(&bytes[i], input_buf, offset + i);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        result->field25 = (bytes[2] << 24) | (bytes[3] << 16) | (bytes[1] << 8) | bytes[0];
        union {
            uint32_t i;
            float f;
        } u;
        u.i = *((uint32_t *)&result->field25);
        result->field25 = u.f;
        offset += 4;
    }

    {
        uint8_t bytes[4];
        for(int i = 0; i < 4; i++) {
            buf_err_t err = buf_get_u8(&bytes[i], input_buf, offset + i);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        result->field26 = bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24);
        union {
            uint32_t i;
            float f;
        } u;
        u.i = *((uint32_t *)&result->field26);
        result->field26 = u.f;
        offset += 4;
    }

    {
        uint8_t bytes[4];
        for(int i = 0; i < 4; i++) {
            buf_err_t err = buf_get_u8(&bytes[i], input_buf, offset + i);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        result->field27 = bytes[1] | (bytes[0] << 8) | (bytes[3] << 16) | (bytes[2] << 24);
        union {
            uint32_t i;
            float f;
        } u;
        u.i = *((uint32_t *)&result->field27);
        result->field27 = u.f;
        offset += 4;
    }

    {
        uint8_t bytes[8];
        for(int i = 0; i < 8; i++) {
            buf_err_t err = buf_get_u8(&bytes[i], input_buf, offset + i);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        result->field28 = ((uint64_t)bytes[0] << 56) | ((uint64_t)bytes[1] << 48) | ((uint64_t)bytes[2] << 40)
                          | ((uint64_t)bytes[3] << 32) | ((uint64_t)bytes[4] << 24) | ((uint64_t)bytes[5] << 16)
                          | ((uint64_t)bytes[6] << 8) | (uint64_t)bytes[7];
        union {
            uint64_t i;
            double f;
        } u;
        u.i = *((uint64_t *)&result->field28);
        result->field28 = u.f;
        offset += 8;
    }

    {
        uint8_t bytes[8];
        for(int i = 0; i < 8; i++) {
            buf_err_t err = buf_get_u8(&bytes[i], input_buf, offset + i);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        result->field29 = ((uint64_t)bytes[6] << 56) | ((uint64_t)bytes[7] << 48) | ((uint64_t)bytes[4] << 40)
                          | ((uint64_t)bytes[5] << 32) | ((uint64_t)bytes[2] << 24) | ((uint64_t)bytes[3] << 16)
                          | ((uint64_t)bytes[0] << 8) | (uint64_t)bytes[1];
        union {
            uint64_t i;
            double f;
        } u;
        u.i = *((uint64_t *)&result->field29);
        result->field29 = u.f;
        offset += 8;
    }

    {
        uint8_t bytes[8];
        for(int i = 0; i < 8; i++) {
            buf_err_t err = buf_get_u8(&bytes[i], input_buf, offset + i);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        result->field30 = (uint64_t)bytes[0] | ((uint64_t)bytes[1] << 8) | ((uint64_t)bytes[2] << 16) | ((uint64_t)bytes[3] << 24)
                          | ((uint64_t)bytes[4] << 32) | ((uint64_t)bytes[5] << 40) | ((uint64_t)bytes[6] << 48)
                          | ((uint64_t)bytes[7] << 56);
        union {
            uint64_t i;
            double f;
        } u;
        u.i = *((uint64_t *)&result->field30);
        result->field30 = u.f;
        offset += 8;
    }

    {
        uint8_t bytes[8];
        for(int i = 0; i < 8; i++) {
            buf_err_t err = buf_get_u8(&bytes[i], input_buf, offset + i);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        result->field31 = (uint64_t)bytes[1] | ((uint64_t)bytes[0] << 8) | ((uint64_t)bytes[3] << 16) | ((uint64_t)bytes[2] << 24)
                          | ((uint64_t)bytes[5] << 32) | ((uint64_t)bytes[4] << 40) | ((uint64_t)bytes[7] << 48)
                          | ((uint64_t)bytes[6] << 56);
        union {
            uint64_t i;
            double f;
        } u;
        u.i = *((uint64_t *)&result->field31);
        result->field31 = u.f;
        offset += 8;
    }

    // Set remaining buffer
    buf_err_t buf_err = buf_from_buf(remaining_input_buf, input_buf, offset, buf_len(input_buf) - offset);
    if(buf_err != BUF_OK) {
        one_of_each_dispose(result);
        if(buf_err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
        if(buf_err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
    }

    *value = result;
    one_of_each_log_info(result);
    return CODEC_OK;
}

codec_err_t one_of_each_encode(buf *remaining_output_buf, buf *output_buf, const struct one_of_each *value) {
    if(!remaining_output_buf || !output_buf || !value) { return CODEC_ERR_NULL_PTR; }

    size_t offset = 0;

    one_of_each_log_info((struct one_of_each *)value);

    buf_err_t err = buf_set_u8(output_buf, offset, (uint8_t)value->field1);
    if(err != BUF_OK) {
        if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
        if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
    }
    offset += 1;

    buf_err_t err = buf_set_u8(output_buf, offset, (uint8_t)value->field2);
    if(err != BUF_OK) {
        if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
        if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
    }
    offset += 1;

    {
        uint8_t bytes[2];
        bytes[0] = (value->field3 >> 8) & 0xFF;
        bytes[1] = value->field3 & 0xFF;
        for(int i = 0; i < 2; i++) {
            buf_err_t err = buf_set_u8(output_buf, offset + i, bytes[i]);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        offset += 2;
    }

    {
        uint8_t bytes[2];
        bytes[0] = (value->field4 >> 8) & 0xFF;
        bytes[1] = value->field4 & 0xFF;
        for(int i = 0; i < 2; i++) {
            buf_err_t err = buf_set_u8(output_buf, offset + i, bytes[i]);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        offset += 2;
    }

    {
        uint8_t bytes[2];
        bytes[0] = value->field5 & 0xFF;
        bytes[1] = (value->field5 >> 8) & 0xFF;
        for(int i = 0; i < 2; i++) {
            buf_err_t err = buf_set_u8(output_buf, offset + i, bytes[i]);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        offset += 2;
    }

    {
        uint8_t bytes[2];
        bytes[0] = value->field6 & 0xFF;
        bytes[1] = (value->field6 >> 8) & 0xFF;
        for(int i = 0; i < 2; i++) {
            buf_err_t err = buf_set_u8(output_buf, offset + i, bytes[i]);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        offset += 2;
    }

    {
        uint8_t bytes[4];
        bytes[0] = (value->field7 >> 24) & 0xFF;
        bytes[1] = (value->field7 >> 16) & 0xFF;
        bytes[2] = (value->field7 >> 8) & 0xFF;
        bytes[3] = value->field7 & 0xFF;
        for(int i = 0; i < 4; i++) {
            buf_err_t err = buf_set_u8(output_buf, offset + i, bytes[i]);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        offset += 4;
    }

    {
        uint8_t bytes[4];
        bytes[0] = (value->field8 >> 24) & 0xFF;
        bytes[1] = (value->field8 >> 16) & 0xFF;
        bytes[2] = (value->field8 >> 8) & 0xFF;
        bytes[3] = value->field8 & 0xFF;
        for(int i = 0; i < 4; i++) {
            buf_err_t err = buf_set_u8(output_buf, offset + i, bytes[i]);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        offset += 4;
    }

    {
        uint8_t bytes[4];
        bytes[2] = (value->field9 >> 24) & 0xFF;
        bytes[3] = (value->field9 >> 16) & 0xFF;
        bytes[1] = (value->field9 >> 8) & 0xFF;
        bytes[0] = value->field9 & 0xFF;
        for(int i = 0; i < 4; i++) {
            buf_err_t err = buf_set_u8(output_buf, offset + i, bytes[i]);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        offset += 4;
    }

    {
        uint8_t bytes[4];
        bytes[2] = (value->field10 >> 24) & 0xFF;
        bytes[3] = (value->field10 >> 16) & 0xFF;
        bytes[1] = (value->field10 >> 8) & 0xFF;
        bytes[0] = value->field10 & 0xFF;
        for(int i = 0; i < 4; i++) {
            buf_err_t err = buf_set_u8(output_buf, offset + i, bytes[i]);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        offset += 4;
    }

    {
        uint8_t bytes[4];
        bytes[0] = value->field11 & 0xFF;
        bytes[1] = (value->field11 >> 8) & 0xFF;
        bytes[2] = (value->field11 >> 16) & 0xFF;
        bytes[3] = (value->field11 >> 24) & 0xFF;
        for(int i = 0; i < 4; i++) {
            buf_err_t err = buf_set_u8(output_buf, offset + i, bytes[i]);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        offset += 4;
    }

    {
        uint8_t bytes[4];
        bytes[0] = value->field12 & 0xFF;
        bytes[1] = (value->field12 >> 8) & 0xFF;
        bytes[2] = (value->field12 >> 16) & 0xFF;
        bytes[3] = (value->field12 >> 24) & 0xFF;
        for(int i = 0; i < 4; i++) {
            buf_err_t err = buf_set_u8(output_buf, offset + i, bytes[i]);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        offset += 4;
    }

    {
        uint8_t bytes[4];
        bytes[1] = value->field13 & 0xFF;
        bytes[0] = (value->field13 >> 8) & 0xFF;
        bytes[3] = (value->field13 >> 16) & 0xFF;
        bytes[2] = (value->field13 >> 24) & 0xFF;
        for(int i = 0; i < 4; i++) {
            buf_err_t err = buf_set_u8(output_buf, offset + i, bytes[i]);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        offset += 4;
    }

    {
        uint8_t bytes[4];
        bytes[1] = value->field15 & 0xFF;
        bytes[0] = (value->field15 >> 8) & 0xFF;
        bytes[3] = (value->field15 >> 16) & 0xFF;
        bytes[2] = (value->field15 >> 24) & 0xFF;
        for(int i = 0; i < 4; i++) {
            buf_err_t err = buf_set_u8(output_buf, offset + i, bytes[i]);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        offset += 4;
    }

    {
        uint8_t bytes[8];
        bytes[0] = (value->field16 >> 56) & 0xFF;
        bytes[1] = (value->field16 >> 48) & 0xFF;
        bytes[2] = (value->field16 >> 40) & 0xFF;
        bytes[3] = (value->field16 >> 32) & 0xFF;
        bytes[4] = (value->field16 >> 24) & 0xFF;
        bytes[5] = (value->field16 >> 16) & 0xFF;
        bytes[6] = (value->field16 >> 8) & 0xFF;
        bytes[7] = (value->field16 >> 0) & 0xFF;
        for(int i = 0; i < 8; i++) {
            buf_err_t err = buf_set_u8(output_buf, offset + i, bytes[i]);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        offset += 8;
    }

    {
        uint8_t bytes[8];
        bytes[0] = (value->field17 >> 56) & 0xFF;
        bytes[1] = (value->field17 >> 48) & 0xFF;
        bytes[2] = (value->field17 >> 40) & 0xFF;
        bytes[3] = (value->field17 >> 32) & 0xFF;
        bytes[4] = (value->field17 >> 24) & 0xFF;
        bytes[5] = (value->field17 >> 16) & 0xFF;
        bytes[6] = (value->field17 >> 8) & 0xFF;
        bytes[7] = (value->field17 >> 0) & 0xFF;
        for(int i = 0; i < 8; i++) {
            buf_err_t err = buf_set_u8(output_buf, offset + i, bytes[i]);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        offset += 8;
    }

    {
        uint8_t bytes[8];
        bytes[6] = (value->field18 >> 56) & 0xFF;
        bytes[7] = (value->field18 >> 48) & 0xFF;
        bytes[4] = (value->field18 >> 40) & 0xFF;
        bytes[5] = (value->field18 >> 32) & 0xFF;
        bytes[2] = (value->field18 >> 24) & 0xFF;
        bytes[3] = (value->field18 >> 16) & 0xFF;
        bytes[0] = (value->field18 >> 8) & 0xFF;
        bytes[1] = (value->field18 >> 0) & 0xFF;
        for(int i = 0; i < 8; i++) {
            buf_err_t err = buf_set_u8(output_buf, offset + i, bytes[i]);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        offset += 8;
    }

    {
        uint8_t bytes[8];
        bytes[6] = (value->field19 >> 56) & 0xFF;
        bytes[7] = (value->field19 >> 48) & 0xFF;
        bytes[4] = (value->field19 >> 40) & 0xFF;
        bytes[5] = (value->field19 >> 32) & 0xFF;
        bytes[2] = (value->field19 >> 24) & 0xFF;
        bytes[3] = (value->field19 >> 16) & 0xFF;
        bytes[0] = (value->field19 >> 8) & 0xFF;
        bytes[1] = (value->field19 >> 0) & 0xFF;
        for(int i = 0; i < 8; i++) {
            buf_err_t err = buf_set_u8(output_buf, offset + i, bytes[i]);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        offset += 8;
    }

    {
        uint8_t bytes[8];
        bytes[0] = (value->field20 >> 0) & 0xFF;
        bytes[1] = (value->field20 >> 8) & 0xFF;
        bytes[2] = (value->field20 >> 16) & 0xFF;
        bytes[3] = (value->field20 >> 24) & 0xFF;
        bytes[4] = (value->field20 >> 32) & 0xFF;
        bytes[5] = (value->field20 >> 40) & 0xFF;
        bytes[6] = (value->field20 >> 48) & 0xFF;
        bytes[7] = (value->field20 >> 56) & 0xFF;
        for(int i = 0; i < 8; i++) {
            buf_err_t err = buf_set_u8(output_buf, offset + i, bytes[i]);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        offset += 8;
    }

    {
        uint8_t bytes[8];
        bytes[0] = (value->field21 >> 0) & 0xFF;
        bytes[1] = (value->field21 >> 8) & 0xFF;
        bytes[2] = (value->field21 >> 16) & 0xFF;
        bytes[3] = (value->field21 >> 24) & 0xFF;
        bytes[4] = (value->field21 >> 32) & 0xFF;
        bytes[5] = (value->field21 >> 40) & 0xFF;
        bytes[6] = (value->field21 >> 48) & 0xFF;
        bytes[7] = (value->field21 >> 56) & 0xFF;
        for(int i = 0; i < 8; i++) {
            buf_err_t err = buf_set_u8(output_buf, offset + i, bytes[i]);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        offset += 8;
    }

    {
        uint8_t bytes[8];
        bytes[1] = (value->field22 >> 0) & 0xFF;
        bytes[0] = (value->field22 >> 8) & 0xFF;
        bytes[3] = (value->field22 >> 16) & 0xFF;
        bytes[2] = (value->field22 >> 24) & 0xFF;
        bytes[5] = (value->field22 >> 32) & 0xFF;
        bytes[4] = (value->field22 >> 40) & 0xFF;
        bytes[7] = (value->field22 >> 48) & 0xFF;
        bytes[6] = (value->field22 >> 56) & 0xFF;
        for(int i = 0; i < 8; i++) {
            buf_err_t err = buf_set_u8(output_buf, offset + i, bytes[i]);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        offset += 8;
    }

    {
        uint8_t bytes[8];
        bytes[1] = (value->field23 >> 0) & 0xFF;
        bytes[0] = (value->field23 >> 8) & 0xFF;
        bytes[3] = (value->field23 >> 16) & 0xFF;
        bytes[2] = (value->field23 >> 24) & 0xFF;
        bytes[5] = (value->field23 >> 32) & 0xFF;
        bytes[4] = (value->field23 >> 40) & 0xFF;
        bytes[7] = (value->field23 >> 48) & 0xFF;
        bytes[6] = (value->field23 >> 56) & 0xFF;
        for(int i = 0; i < 8; i++) {
            buf_err_t err = buf_set_u8(output_buf, offset + i, bytes[i]);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        offset += 8;
    }

    {
        uint8_t bytes[4];
        union {
            uint32_t i;
            float f;
        } u;
        u.f = value->field24;
        bytes[0] = (u.i >> 24) & 0xFF;
        bytes[1] = (u.i >> 16) & 0xFF;
        bytes[2] = (u.i >> 8) & 0xFF;
        bytes[3] = u.i & 0xFF;
        for(int i = 0; i < 4; i++) {
            buf_err_t err = buf_set_u8(output_buf, offset + i, bytes[i]);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        offset += 4;
    }

    {
        uint8_t bytes[4];
        union {
            uint32_t i;
            float f;
        } u;
        u.f = value->field25;
        bytes[2] = (u.i >> 24) & 0xFF;
        bytes[3] = (u.i >> 16) & 0xFF;
        bytes[1] = (u.i >> 8) & 0xFF;
        bytes[0] = u.i & 0xFF;
        for(int i = 0; i < 4; i++) {
            buf_err_t err = buf_set_u8(output_buf, offset + i, bytes[i]);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        offset += 4;
    }

    {
        uint8_t bytes[4];
        union {
            uint32_t i;
            float f;
        } u;
        u.f = value->field26;
        bytes[0] = u.i & 0xFF;
        bytes[1] = (u.i >> 8) & 0xFF;
        bytes[2] = (u.i >> 16) & 0xFF;
        bytes[3] = (u.i >> 24) & 0xFF;
        for(int i = 0; i < 4; i++) {
            buf_err_t err = buf_set_u8(output_buf, offset + i, bytes[i]);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        offset += 4;
    }

    {
        uint8_t bytes[4];
        union {
            uint32_t i;
            float f;
        } u;
        u.f = value->field27;
        bytes[1] = u.i & 0xFF;
        bytes[0] = (u.i >> 8) & 0xFF;
        bytes[3] = (u.i >> 16) & 0xFF;
        bytes[2] = (u.i >> 24) & 0xFF;
        for(int i = 0; i < 4; i++) {
            buf_err_t err = buf_set_u8(output_buf, offset + i, bytes[i]);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        offset += 4;
    }

    {
        uint8_t bytes[8];
        union {
            uint64_t i;
            double f;
        } u;
        u.f = value->field28;
        bytes[0] = (u.i >> 56) & 0xFF;
        bytes[1] = (u.i >> 48) & 0xFF;
        bytes[2] = (u.i >> 40) & 0xFF;
        bytes[3] = (u.i >> 32) & 0xFF;
        bytes[4] = (u.i >> 24) & 0xFF;
        bytes[5] = (u.i >> 16) & 0xFF;
        bytes[6] = (u.i >> 8) & 0xFF;
        bytes[7] = (u.i >> 0) & 0xFF;
        for(int i = 0; i < 8; i++) {
            buf_err_t err = buf_set_u8(output_buf, offset + i, bytes[i]);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        offset += 8;
    }

    {
        uint8_t bytes[8];
        union {
            uint64_t i;
            double f;
        } u;
        u.f = value->field29;
        bytes[6] = (u.i >> 56) & 0xFF;
        bytes[7] = (u.i >> 48) & 0xFF;
        bytes[4] = (u.i >> 40) & 0xFF;
        bytes[5] = (u.i >> 32) & 0xFF;
        bytes[2] = (u.i >> 24) & 0xFF;
        bytes[3] = (u.i >> 16) & 0xFF;
        bytes[0] = (u.i >> 8) & 0xFF;
        bytes[1] = (u.i >> 0) & 0xFF;
        for(int i = 0; i < 8; i++) {
            buf_err_t err = buf_set_u8(output_buf, offset + i, bytes[i]);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        offset += 8;
    }

    {
        uint8_t bytes[8];
        union {
            uint64_t i;
            double f;
        } u;
        u.f = value->field30;
        bytes[0] = (u.i >> 0) & 0xFF;
        bytes[1] = (u.i >> 8) & 0xFF;
        bytes[2] = (u.i >> 16) & 0xFF;
        bytes[3] = (u.i >> 24) & 0xFF;
        bytes[4] = (u.i >> 32) & 0xFF;
        bytes[5] = (u.i >> 40) & 0xFF;
        bytes[6] = (u.i >> 48) & 0xFF;
        bytes[7] = (u.i >> 56) & 0xFF;
        for(int i = 0; i < 8; i++) {
            buf_err_t err = buf_set_u8(output_buf, offset + i, bytes[i]);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        offset += 8;
    }

    {
        uint8_t bytes[8];
        union {
            uint64_t i;
            double f;
        } u;
        u.f = value->field31;
        bytes[1] = (u.i >> 0) & 0xFF;
        bytes[0] = (u.i >> 8) & 0xFF;
        bytes[3] = (u.i >> 16) & 0xFF;
        bytes[2] = (u.i >> 24) & 0xFF;
        bytes[5] = (u.i >> 32) & 0xFF;
        bytes[4] = (u.i >> 40) & 0xFF;
        bytes[7] = (u.i >> 48) & 0xFF;
        bytes[6] = (u.i >> 56) & 0xFF;
        for(int i = 0; i < 8; i++) {
            buf_err_t err = buf_set_u8(output_buf, offset + i, bytes[i]);
            if(err != BUF_OK) {
                if(err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
                if(err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
            }
        }
        offset += 8;
    }

    // Set remaining buffer
    buf_err_t buf_err = buf_from_buf(remaining_output_buf, output_buf, offset, buf_len(output_buf) - offset);
    if(buf_err != BUF_OK) {
        if(buf_err == BUF_ERR_OUT_OF_BOUNDS) { return CODEC_ERR_OUT_OF_BOUNDS; }
        if(buf_err == BUF_ERR_NULL_PTR) { return CODEC_ERR_NULL_PTR; }
    }

    info_buf("Encoded one_of_each output", buf_data(output_buf), offset);
    return CODEC_OK;
}

void one_of_each_dispose(struct one_of_each *value) {
    if(!value) {
        warn("Called one_of_each_dispose with NULL pointer");
        return;
    }

    info("Disposing one_of_each");

    free(value);
}

void one_of_each_log_impl(const char *func, int line_num, ev_log_level log_level, struct one_of_each *value) {
    if(!value) {
        ev_log_impl(func, line_num, log_level, "one_of_each: NULL");
        return;
    }

    ev_log_impl(func, line_num, log_level, "field1: 0x%02X", value->field1);
    ev_log_impl(func, line_num, log_level, "field2: 0x%02X", value->field2);
    ev_log_impl(func, line_num, log_level, "field3: 0x%04X", value->field3);
    ev_log_impl(func, line_num, log_level, "field4: 0x%04X", value->field4);
    ev_log_impl(func, line_num, log_level, "field5: 0x%04X", value->field5);
    ev_log_impl(func, line_num, log_level, "field6: 0x%04X", value->field6);
    ev_log_impl(func, line_num, log_level, "field7: 0x%08X", value->field7);
    ev_log_impl(func, line_num, log_level, "field8: 0x%08X", value->field8);
    ev_log_impl(func, line_num, log_level, "field9: 0x%08X", value->field9);
    ev_log_impl(func, line_num, log_level, "field10: 0x%08X", value->field10);
    ev_log_impl(func, line_num, log_level, "field11: 0x%08X", value->field11);
    ev_log_impl(func, line_num, log_level, "field12: 0x%08X", value->field12);
    ev_log_impl(func, line_num, log_level, "field13: 0x%08X", value->field13);
    ev_log_impl(func, line_num, log_level, "field15: 0x%08X", value->field15);
    ev_log_impl(func, line_num, log_level, "field16: 0x%016llX", (unsigned long long)value->field16);
    ev_log_impl(func, line_num, log_level, "field17: 0x%016llX", (unsigned long long)value->field17);
    ev_log_impl(func, line_num, log_level, "field18: 0x%016llX", (unsigned long long)value->field18);
    ev_log_impl(func, line_num, log_level, "field19: 0x%016llX", (unsigned long long)value->field19);
    ev_log_impl(func, line_num, log_level, "field20: 0x%016llX", (unsigned long long)value->field20);
    ev_log_impl(func, line_num, log_level, "field21: 0x%016llX", (unsigned long long)value->field21);
    ev_log_impl(func, line_num, log_level, "field22: 0x%016llX", (unsigned long long)value->field22);
    ev_log_impl(func, line_num, log_level, "field23: 0x%016llX", (unsigned long long)value->field23);
    ev_log_impl(func, line_num, log_level, "field24: %.6f", value->field24);
    ev_log_impl(func, line_num, log_level, "field25: %.6f", value->field25);
    ev_log_impl(func, line_num, log_level, "field26: %.6f", value->field26);
    ev_log_impl(func, line_num, log_level, "field27: %.6f", value->field27);
    ev_log_impl(func, line_num, log_level, "field28: %.6f", value->field28);
    ev_log_impl(func, line_num, log_level, "field29: %.6f", value->field29);
    ev_log_impl(func, line_num, log_level, "field30: %.6f", value->field30);
    ev_log_impl(func, line_num, log_level, "field31: %.6f", value->field31);
}
