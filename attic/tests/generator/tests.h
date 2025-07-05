#ifndef TESTS_H
#define TESTS_H

#include <stdint.h>
#include <stddef.h>
#include <log.h>
#include <buf.h>

/* Error handling enum */
typedef enum {
    CODEC_OK = 0,                    // Success
    CODEC_ERR_OUT_OF_BOUNDS,         // Buffer bounds exceeded
    CODEC_ERR_NULL_PTR,              // Null pointer passed
    CODEC_ERR_NO_MEMORY,             // Memory allocation failed
} codec_err_t;

/* Codec type definitions */
typedef float codec_f32_be;
typedef float codec_f32_be_bs;
typedef float codec_f32_le;
typedef float codec_f32_le_bs;
typedef double codec_f64_be;
typedef double codec_f64_be_bs;
typedef double codec_f64_le;
typedef double codec_f64_le_bs;
typedef int16_t codec_i16_be;
typedef int16_t codec_i16_le;
typedef int32_t codec_i32_be;
typedef int32_t codec_i32_be_bs;
typedef int32_t codec_i32_le;
typedef int32_t codec_i32_le_bs;
typedef int64_t codec_i64_be;
typedef int64_t codec_i64_be_bs;
typedef int64_t codec_i64_le;
typedef int64_t codec_i64_le_bs;
typedef int8_t codec_i8;
typedef uint16_t codec_u16_be;
typedef uint16_t codec_u16_le;
typedef uint32_t codec_u32_be;
typedef uint32_t codec_u32_be_bs;
typedef uint32_t codec_u32_le;
typedef uint32_t codec_u32_le_bs;
typedef uint64_t codec_u64_be;
typedef uint64_t codec_u64_be_bs;
typedef uint64_t codec_u64_le;
typedef uint64_t codec_u64_le_bs;
typedef uint8_t codec_u8;

/* Original definitions */
#define ARRAY_LEN (10)

struct simple {
    codec_u32_be_bs foo;
    codec_i16_le bar;
};

struct embedding_simple {
    codec_u8 id;
    struct simple;
    codec_f64_le float_field;
};

struct array_test {
    codec_i64_be i64_be_array[ARRAY_LEN];
};

struct simple_pointer_test {
    codec_u32_le len;
    codec_u8 *data;
};

struct array_of_strings_test {
    codec_u16_le num_strings;
    codec_u8 *array_of_c_strings;
};

struct one_of_each {
    codec_u8 field1;
    codec_i8 field2;
    codec_u16_be field3;
    codec_i16_be field4;
    codec_u16_le field5;
    codec_i16_le field6;
    codec_u32_be field7;
    codec_i32_be field8;
    codec_u32_be_bs field9;
    codec_i32_be_bs field10;
    codec_u32_le field11;
    codec_i32_le field12;
    codec_u32_le_bs field13;
    codec_i32_le_bs field15;
    codec_u64_be field16;
    codec_i64_be field17;
    codec_u64_be_bs field18;
    codec_i64_be_bs field19;
    codec_u64_le field20;
    codec_i64_le field21;
    codec_u64_le_bs field22;
    codec_i64_le_bs field23;
    codec_f32_be field24;
    codec_f32_be_bs field25;
    codec_f32_le field26;
    codec_f32_le_bs field27;
    codec_f64_be field28;
    codec_f64_be_bs field29;
    codec_f64_le field30;
    codec_f64_le_bs field31;
};

/* Generated function declarations */
/* functions for struct simple */
codec_err_t simple_decode(struct simple **value, buf *remaining_input_buf, buf *input_buf);
codec_err_t simple_encode(buf *remaining_output_buf, buf *output_buf, const struct simple *value);
void simple_dispose(struct simple *value);
void simple_log_impl(const char *func, int line_num, ev_log_level log_level, struct simple *value);

/* functions for struct embedding_simple */
codec_err_t embedding_simple_decode(struct embedding_simple **value, buf *remaining_input_buf, buf *input_buf);
codec_err_t embedding_simple_encode(buf *remaining_output_buf, buf *output_buf, const struct embedding_simple *value);
void embedding_simple_dispose(struct embedding_simple *value);
void embedding_simple_log_impl(const char *func, int line_num, ev_log_level log_level, struct embedding_simple *value);

/* functions for struct array_test */
codec_err_t array_test_decode(struct array_test **value, buf *remaining_input_buf, buf *input_buf);
codec_err_t array_test_encode(buf *remaining_output_buf, buf *output_buf, const struct array_test *value);
void array_test_dispose(struct array_test *value);
void array_test_log_impl(const char *func, int line_num, ev_log_level log_level, struct array_test *value);

/* functions for struct simple_pointer_test */
codec_err_t simple_pointer_test_decode(struct simple_pointer_test **value, buf *remaining_input_buf, buf *input_buf);
codec_err_t simple_pointer_test_encode(buf *remaining_output_buf, buf *output_buf, const struct simple_pointer_test *value);
void simple_pointer_test_dispose(struct simple_pointer_test *value);
void simple_pointer_test_log_impl(const char *func, int line_num, ev_log_level log_level, struct simple_pointer_test *value);

/* user-defined functions for pointer field data */
codec_err_t simple_pointer_test_data_decode(struct simple_pointer_test *value, buf *remaining_input_buf, buf *input_buf);
codec_err_t simple_pointer_test_data_encode(buf *remaining_output_buf, buf *output_buf, const struct simple_pointer_test *value);
void simple_pointer_test_data_dispose(struct simple_pointer_test *value);
void simple_pointer_test_data_log_impl(const char *func, int line_num, ev_log_level log_level, struct simple_pointer_test *value);

/* functions for struct array_of_strings_test */
codec_err_t array_of_strings_test_decode(struct array_of_strings_test **value, buf *remaining_input_buf, buf *input_buf);
codec_err_t array_of_strings_test_encode(buf *remaining_output_buf, buf *output_buf, const struct array_of_strings_test *value);
void array_of_strings_test_dispose(struct array_of_strings_test *value);
void array_of_strings_test_log_impl(const char *func, int line_num, ev_log_level log_level, struct array_of_strings_test *value);

/* user-defined functions for pointer field array_of_c_strings */
codec_err_t array_of_strings_test_array_of_c_strings_decode(struct array_of_strings_test *value, buf *remaining_input_buf, buf *input_buf);
codec_err_t array_of_strings_test_array_of_c_strings_encode(buf *remaining_output_buf, buf *output_buf, const struct array_of_strings_test *value);
void array_of_strings_test_array_of_c_strings_dispose(struct array_of_strings_test *value);
void array_of_strings_test_array_of_c_strings_log_impl(const char *func, int line_num, ev_log_level log_level, struct array_of_strings_test *value);

/* functions for struct one_of_each */
codec_err_t one_of_each_decode(struct one_of_each **value, buf *remaining_input_buf, buf *input_buf);
codec_err_t one_of_each_encode(buf *remaining_output_buf, buf *output_buf, const struct one_of_each *value);
void one_of_each_dispose(struct one_of_each *value);
void one_of_each_log_impl(const char *func, int line_num, ev_log_level log_level, struct one_of_each *value);

/* Logging macros */
/* logging macros for struct simple */
#define simple_log_error(value) do { if(ev_log_level_get() <= EV_LOG_LEVEL_ERROR) simple_log_impl(__func__, __LINE__, EV_LOG_LEVEL_ERROR, value); } while(0)
#define simple_log_warn(value) do { if(ev_log_level_get() <= EV_LOG_LEVEL_WARN) simple_log_impl(__func__, __LINE__, EV_LOG_LEVEL_WARN, value); } while(0)
#define simple_log_info(value) do { if(ev_log_level_get() <= EV_LOG_LEVEL_INFO) simple_log_impl(__func__, __LINE__, EV_LOG_LEVEL_INFO, value); } while(0)
#define simple_log_debug(value) do { if(ev_log_level_get() <= EV_LOG_LEVEL_DEBUG) simple_log_impl(__func__, __LINE__, EV_LOG_LEVEL_DEBUG, value); } while(0)
#define simple_log_trace(value) do { if(ev_log_level_get() <= EV_LOG_LEVEL_TRACE) simple_log_impl(__func__, __LINE__, EV_LOG_LEVEL_TRACE, value); } while(0)

/* logging macros for struct embedding_simple */
#define embedding_simple_log_error(value) do { if(ev_log_level_get() <= EV_LOG_LEVEL_ERROR) embedding_simple_log_impl(__func__, __LINE__, EV_LOG_LEVEL_ERROR, value); } while(0)
#define embedding_simple_log_warn(value) do { if(ev_log_level_get() <= EV_LOG_LEVEL_WARN) embedding_simple_log_impl(__func__, __LINE__, EV_LOG_LEVEL_WARN, value); } while(0)
#define embedding_simple_log_info(value) do { if(ev_log_level_get() <= EV_LOG_LEVEL_INFO) embedding_simple_log_impl(__func__, __LINE__, EV_LOG_LEVEL_INFO, value); } while(0)
#define embedding_simple_log_debug(value) do { if(ev_log_level_get() <= EV_LOG_LEVEL_DEBUG) embedding_simple_log_impl(__func__, __LINE__, EV_LOG_LEVEL_DEBUG, value); } while(0)
#define embedding_simple_log_trace(value) do { if(ev_log_level_get() <= EV_LOG_LEVEL_TRACE) embedding_simple_log_impl(__func__, __LINE__, EV_LOG_LEVEL_TRACE, value); } while(0)

/* logging macros for struct array_test */
#define array_test_log_error(value) do { if(ev_log_level_get() <= EV_LOG_LEVEL_ERROR) array_test_log_impl(__func__, __LINE__, EV_LOG_LEVEL_ERROR, value); } while(0)
#define array_test_log_warn(value) do { if(ev_log_level_get() <= EV_LOG_LEVEL_WARN) array_test_log_impl(__func__, __LINE__, EV_LOG_LEVEL_WARN, value); } while(0)
#define array_test_log_info(value) do { if(ev_log_level_get() <= EV_LOG_LEVEL_INFO) array_test_log_impl(__func__, __LINE__, EV_LOG_LEVEL_INFO, value); } while(0)
#define array_test_log_debug(value) do { if(ev_log_level_get() <= EV_LOG_LEVEL_DEBUG) array_test_log_impl(__func__, __LINE__, EV_LOG_LEVEL_DEBUG, value); } while(0)
#define array_test_log_trace(value) do { if(ev_log_level_get() <= EV_LOG_LEVEL_TRACE) array_test_log_impl(__func__, __LINE__, EV_LOG_LEVEL_TRACE, value); } while(0)

/* logging macros for struct simple_pointer_test */
#define simple_pointer_test_log_error(value) do { if(ev_log_level_get() <= EV_LOG_LEVEL_ERROR) simple_pointer_test_log_impl(__func__, __LINE__, EV_LOG_LEVEL_ERROR, value); } while(0)
#define simple_pointer_test_log_warn(value) do { if(ev_log_level_get() <= EV_LOG_LEVEL_WARN) simple_pointer_test_log_impl(__func__, __LINE__, EV_LOG_LEVEL_WARN, value); } while(0)
#define simple_pointer_test_log_info(value) do { if(ev_log_level_get() <= EV_LOG_LEVEL_INFO) simple_pointer_test_log_impl(__func__, __LINE__, EV_LOG_LEVEL_INFO, value); } while(0)
#define simple_pointer_test_log_debug(value) do { if(ev_log_level_get() <= EV_LOG_LEVEL_DEBUG) simple_pointer_test_log_impl(__func__, __LINE__, EV_LOG_LEVEL_DEBUG, value); } while(0)
#define simple_pointer_test_log_trace(value) do { if(ev_log_level_get() <= EV_LOG_LEVEL_TRACE) simple_pointer_test_log_impl(__func__, __LINE__, EV_LOG_LEVEL_TRACE, value); } while(0)

/* logging macros for pointer field data */
#define simple_pointer_test_data_log_error(value) do { if(ev_log_level_get() <= EV_LOG_LEVEL_ERROR) simple_pointer_test_data_log_impl(__func__, __LINE__, EV_LOG_LEVEL_ERROR, value); } while(0)
#define simple_pointer_test_data_log_warn(value) do { if(ev_log_level_get() <= EV_LOG_LEVEL_WARN) simple_pointer_test_data_log_impl(__func__, __LINE__, EV_LOG_LEVEL_WARN, value); } while(0)
#define simple_pointer_test_data_log_info(value) do { if(ev_log_level_get() <= EV_LOG_LEVEL_INFO) simple_pointer_test_data_log_impl(__func__, __LINE__, EV_LOG_LEVEL_INFO, value); } while(0)
#define simple_pointer_test_data_log_debug(value) do { if(ev_log_level_get() <= EV_LOG_LEVEL_DEBUG) simple_pointer_test_data_log_impl(__func__, __LINE__, EV_LOG_LEVEL_DEBUG, value); } while(0)
#define simple_pointer_test_data_log_trace(value) do { if(ev_log_level_get() <= EV_LOG_LEVEL_TRACE) simple_pointer_test_data_log_impl(__func__, __LINE__, EV_LOG_LEVEL_TRACE, value); } while(0)

/* logging macros for struct array_of_strings_test */
#define array_of_strings_test_log_error(value) do { if(ev_log_level_get() <= EV_LOG_LEVEL_ERROR) array_of_strings_test_log_impl(__func__, __LINE__, EV_LOG_LEVEL_ERROR, value); } while(0)
#define array_of_strings_test_log_warn(value) do { if(ev_log_level_get() <= EV_LOG_LEVEL_WARN) array_of_strings_test_log_impl(__func__, __LINE__, EV_LOG_LEVEL_WARN, value); } while(0)
#define array_of_strings_test_log_info(value) do { if(ev_log_level_get() <= EV_LOG_LEVEL_INFO) array_of_strings_test_log_impl(__func__, __LINE__, EV_LOG_LEVEL_INFO, value); } while(0)
#define array_of_strings_test_log_debug(value) do { if(ev_log_level_get() <= EV_LOG_LEVEL_DEBUG) array_of_strings_test_log_impl(__func__, __LINE__, EV_LOG_LEVEL_DEBUG, value); } while(0)
#define array_of_strings_test_log_trace(value) do { if(ev_log_level_get() <= EV_LOG_LEVEL_TRACE) array_of_strings_test_log_impl(__func__, __LINE__, EV_LOG_LEVEL_TRACE, value); } while(0)

/* logging macros for pointer field array_of_c_strings */
#define array_of_strings_test_array_of_c_strings_log_error(value) do { if(ev_log_level_get() <= EV_LOG_LEVEL_ERROR) array_of_strings_test_array_of_c_strings_log_impl(__func__, __LINE__, EV_LOG_LEVEL_ERROR, value); } while(0)
#define array_of_strings_test_array_of_c_strings_log_warn(value) do { if(ev_log_level_get() <= EV_LOG_LEVEL_WARN) array_of_strings_test_array_of_c_strings_log_impl(__func__, __LINE__, EV_LOG_LEVEL_WARN, value); } while(0)
#define array_of_strings_test_array_of_c_strings_log_info(value) do { if(ev_log_level_get() <= EV_LOG_LEVEL_INFO) array_of_strings_test_array_of_c_strings_log_impl(__func__, __LINE__, EV_LOG_LEVEL_INFO, value); } while(0)
#define array_of_strings_test_array_of_c_strings_log_debug(value) do { if(ev_log_level_get() <= EV_LOG_LEVEL_DEBUG) array_of_strings_test_array_of_c_strings_log_impl(__func__, __LINE__, EV_LOG_LEVEL_DEBUG, value); } while(0)
#define array_of_strings_test_array_of_c_strings_log_trace(value) do { if(ev_log_level_get() <= EV_LOG_LEVEL_TRACE) array_of_strings_test_array_of_c_strings_log_impl(__func__, __LINE__, EV_LOG_LEVEL_TRACE, value); } while(0)

/* logging macros for struct one_of_each */
#define one_of_each_log_error(value) do { if(ev_log_level_get() <= EV_LOG_LEVEL_ERROR) one_of_each_log_impl(__func__, __LINE__, EV_LOG_LEVEL_ERROR, value); } while(0)
#define one_of_each_log_warn(value) do { if(ev_log_level_get() <= EV_LOG_LEVEL_WARN) one_of_each_log_impl(__func__, __LINE__, EV_LOG_LEVEL_WARN, value); } while(0)
#define one_of_each_log_info(value) do { if(ev_log_level_get() <= EV_LOG_LEVEL_INFO) one_of_each_log_impl(__func__, __LINE__, EV_LOG_LEVEL_INFO, value); } while(0)
#define one_of_each_log_debug(value) do { if(ev_log_level_get() <= EV_LOG_LEVEL_DEBUG) one_of_each_log_impl(__func__, __LINE__, EV_LOG_LEVEL_DEBUG, value); } while(0)
#define one_of_each_log_trace(value) do { if(ev_log_level_get() <= EV_LOG_LEVEL_TRACE) one_of_each_log_impl(__func__, __LINE__, EV_LOG_LEVEL_TRACE, value); } while(0)

#endif /* TESTS_H */