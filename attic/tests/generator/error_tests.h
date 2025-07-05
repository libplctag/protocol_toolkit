#ifndef ERROR_TESTS_H
#define ERROR_TESTS_H

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
typedef double codec_f64_le_bs;
typedef uint16_t codec_u16_be;
typedef uint16_t codec_u16_le;
typedef uint32_t codec_u32_be;
typedef uint32_t codec_u32_le;
typedef uint64_t codec_u64_be;
typedef uint64_t codec_u64_be_bs;
typedef uint8_t codec_u8;

/* Original definitions */
#define SMALL_BUFFER_SIZE 4
#define LARGE_BUFFER_SIZE 1024

struct minimal_struct {
    codec_u8 byte1;
    codec_u16_be word1;
    codec_u32_le dword1;
};

struct null_test {
    codec_u8 id;
    codec_u8 *null_data;
};

struct large_array_struct {
    codec_u16_be count;
    codec_u64_be large_array[LARGE_BUFFER_SIZE];
};

struct nested_error_test {
    codec_u8 header;
    struct minimal_struct;
    codec_u8 footer;
};

struct mixed_error_test {
    codec_u8 type;
    codec_u32_be length;
    codec_u8 *variable_data;
    struct minimal_struct;
    codec_u16_le checksum;
};

struct boundary_test {
    codec_u8 small_field;
    codec_u64_be_bs large_field;
    codec_f64_le_bs float_field;
};

/* Generated function declarations */
/* functions for struct minimal_struct */
codec_err_t minimal_struct_decode(struct minimal_struct **value, buf *remaining_input_buf, buf *input_buf);
codec_err_t minimal_struct_encode(buf *remaining_output_buf, buf *output_buf, const struct minimal_struct *value);
void minimal_struct_dispose(struct minimal_struct *value);
void minimal_struct_log_impl(const char *func, int line_num, ptk_log_level log_level, struct minimal_struct *value);

/* functions for struct null_test */
codec_err_t null_test_decode(struct null_test **value, buf *remaining_input_buf, buf *input_buf);
codec_err_t null_test_encode(buf *remaining_output_buf, buf *output_buf, const struct null_test *value);
void null_test_dispose(struct null_test *value);
void null_test_log_impl(const char *func, int line_num, ptk_log_level log_level, struct null_test *value);

/* user-defined functions for pointer field null_data */
codec_err_t null_test_null_data_decode(struct null_test *value, buf *remaining_input_buf, buf *input_buf);
codec_err_t null_test_null_data_encode(buf *remaining_output_buf, buf *output_buf, const struct null_test *value);
void null_test_null_data_dispose(struct null_test *value);
void null_test_null_data_log_impl(const char *func, int line_num, ptk_log_level log_level, struct null_test *value);

/* functions for struct large_array_struct */
codec_err_t large_array_struct_decode(struct large_array_struct **value, buf *remaining_input_buf, buf *input_buf);
codec_err_t large_array_struct_encode(buf *remaining_output_buf, buf *output_buf, const struct large_array_struct *value);
void large_array_struct_dispose(struct large_array_struct *value);
void large_array_struct_log_impl(const char *func, int line_num, ptk_log_level log_level, struct large_array_struct *value);

/* functions for struct nested_error_test */
codec_err_t nested_error_test_decode(struct nested_error_test **value, buf *remaining_input_buf, buf *input_buf);
codec_err_t nested_error_test_encode(buf *remaining_output_buf, buf *output_buf, const struct nested_error_test *value);
void nested_error_test_dispose(struct nested_error_test *value);
void nested_error_test_log_impl(const char *func, int line_num, ptk_log_level log_level, struct nested_error_test *value);

/* functions for struct mixed_error_test */
codec_err_t mixed_error_test_decode(struct mixed_error_test **value, buf *remaining_input_buf, buf *input_buf);
codec_err_t mixed_error_test_encode(buf *remaining_output_buf, buf *output_buf, const struct mixed_error_test *value);
void mixed_error_test_dispose(struct mixed_error_test *value);
void mixed_error_test_log_impl(const char *func, int line_num, ptk_log_level log_level, struct mixed_error_test *value);

/* user-defined functions for pointer field variable_data */
codec_err_t mixed_error_test_variable_data_decode(struct mixed_error_test *value, buf *remaining_input_buf, buf *input_buf);
codec_err_t mixed_error_test_variable_data_encode(buf *remaining_output_buf, buf *output_buf, const struct mixed_error_test *value);
void mixed_error_test_variable_data_dispose(struct mixed_error_test *value);
void mixed_error_test_variable_data_log_impl(const char *func, int line_num, ptk_log_level log_level, struct mixed_error_test *value);

/* functions for struct boundary_test */
codec_err_t boundary_test_decode(struct boundary_test **value, buf *remaining_input_buf, buf *input_buf);
codec_err_t boundary_test_encode(buf *remaining_output_buf, buf *output_buf, const struct boundary_test *value);
void boundary_test_dispose(struct boundary_test *value);
void boundary_test_log_impl(const char *func, int line_num, ptk_log_level log_level, struct boundary_test *value);

/* Logging macros */
/* logging macros for struct minimal_struct */
#define minimal_struct_log_error(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_ERROR) minimal_struct_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_ERROR, value); } while(0)
#define minimal_struct_log_warn(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_WARN) minimal_struct_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_WARN, value); } while(0)
#define minimal_struct_log_info(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_INFO) minimal_struct_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_INFO, value); } while(0)
#define minimal_struct_log_debug(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_DEBUG) minimal_struct_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_DEBUG, value); } while(0)
#define minimal_struct_log_trace(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_TRACE) minimal_struct_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_TRACE, value); } while(0)

/* logging macros for struct null_test */
#define null_test_log_error(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_ERROR) null_test_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_ERROR, value); } while(0)
#define null_test_log_warn(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_WARN) null_test_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_WARN, value); } while(0)
#define null_test_log_info(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_INFO) null_test_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_INFO, value); } while(0)
#define null_test_log_debug(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_DEBUG) null_test_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_DEBUG, value); } while(0)
#define null_test_log_trace(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_TRACE) null_test_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_TRACE, value); } while(0)

/* logging macros for pointer field null_data */
#define null_test_null_data_log_error(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_ERROR) null_test_null_data_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_ERROR, value); } while(0)
#define null_test_null_data_log_warn(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_WARN) null_test_null_data_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_WARN, value); } while(0)
#define null_test_null_data_log_info(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_INFO) null_test_null_data_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_INFO, value); } while(0)
#define null_test_null_data_log_debug(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_DEBUG) null_test_null_data_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_DEBUG, value); } while(0)
#define null_test_null_data_log_trace(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_TRACE) null_test_null_data_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_TRACE, value); } while(0)

/* logging macros for struct large_array_struct */
#define large_array_struct_log_error(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_ERROR) large_array_struct_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_ERROR, value); } while(0)
#define large_array_struct_log_warn(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_WARN) large_array_struct_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_WARN, value); } while(0)
#define large_array_struct_log_info(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_INFO) large_array_struct_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_INFO, value); } while(0)
#define large_array_struct_log_debug(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_DEBUG) large_array_struct_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_DEBUG, value); } while(0)
#define large_array_struct_log_trace(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_TRACE) large_array_struct_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_TRACE, value); } while(0)

/* logging macros for struct nested_error_test */
#define nested_error_test_log_error(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_ERROR) nested_error_test_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_ERROR, value); } while(0)
#define nested_error_test_log_warn(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_WARN) nested_error_test_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_WARN, value); } while(0)
#define nested_error_test_log_info(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_INFO) nested_error_test_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_INFO, value); } while(0)
#define nested_error_test_log_debug(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_DEBUG) nested_error_test_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_DEBUG, value); } while(0)
#define nested_error_test_log_trace(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_TRACE) nested_error_test_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_TRACE, value); } while(0)

/* logging macros for struct mixed_error_test */
#define mixed_error_test_log_error(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_ERROR) mixed_error_test_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_ERROR, value); } while(0)
#define mixed_error_test_log_warn(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_WARN) mixed_error_test_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_WARN, value); } while(0)
#define mixed_error_test_log_info(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_INFO) mixed_error_test_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_INFO, value); } while(0)
#define mixed_error_test_log_debug(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_DEBUG) mixed_error_test_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_DEBUG, value); } while(0)
#define mixed_error_test_log_trace(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_TRACE) mixed_error_test_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_TRACE, value); } while(0)

/* logging macros for pointer field variable_data */
#define mixed_error_test_variable_data_log_error(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_ERROR) mixed_error_test_variable_data_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_ERROR, value); } while(0)
#define mixed_error_test_variable_data_log_warn(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_WARN) mixed_error_test_variable_data_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_WARN, value); } while(0)
#define mixed_error_test_variable_data_log_info(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_INFO) mixed_error_test_variable_data_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_INFO, value); } while(0)
#define mixed_error_test_variable_data_log_debug(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_DEBUG) mixed_error_test_variable_data_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_DEBUG, value); } while(0)
#define mixed_error_test_variable_data_log_trace(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_TRACE) mixed_error_test_variable_data_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_TRACE, value); } while(0)

/* logging macros for struct boundary_test */
#define boundary_test_log_error(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_ERROR) boundary_test_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_ERROR, value); } while(0)
#define boundary_test_log_warn(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_WARN) boundary_test_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_WARN, value); } while(0)
#define boundary_test_log_info(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_INFO) boundary_test_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_INFO, value); } while(0)
#define boundary_test_log_debug(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_DEBUG) boundary_test_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_DEBUG, value); } while(0)
#define boundary_test_log_trace(value) do { if(ptk_log_level_get() <= PTK_LOG_LEVEL_TRACE) boundary_test_log_impl(__func__, __LINE__, PTK_LOG_LEVEL_TRACE, value); } while(0)

#endif /* ERROR_TESTS_H */