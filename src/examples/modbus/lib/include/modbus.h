#ifndef MODBUS_API_H
#define MODBUS_API_H

#include <ptk_alloc.h>
#include <ptk_array.h>
#include <ptk_err.h>
#include <ptk_buf.h>
#include <ptk_socket.h>


PTK_ARRAY_DECLARE(modbus_register, uint16_t);
PTK_ARRAY_DECLARE(modbus_bool, bool);

typedef struct modbus_connection modbus_connection;


modbus_connection *modbus_open_client(ptk_allocator_t *allocator, ptk_address_t *addr, uint8_t unit_id, ptk_buf_t *buffer);
modbus_connection *modbus_open_server(ptk_allocator_t *allocator, ptk_address_t *addr, uint8_t unit_id, ptk_buf_t *buffer);
ptk_err modbus_close(modbus_connection *conn);


/* client functions */
ptk_err client_send_read_holding_register_req(modbus_connection *con, uint16_t register_addr);
ptk_err client_send_read_holding_registers_req(modbus_connection *con, uint16_t base_register, uint16_t num_registers);
ptk_err client_send_read_input_register_req(modbus_connection *con, uint16_t register_addr);
ptk_err client_send_read_input_registers_req(modbus_connection *con, uint16_t base_register, uint16_t num_registers);
ptk_err client_send_read_discrete_input_req(modbus_connection *con, uint16_t input_addr);
ptk_err client_send_read_discrete_inputs_req(modbus_connection *con, uint16_t base_input, uint16_t num_inputs);
ptk_err client_send_read_coil_req(modbus_connection *con, uint16_t register_addr);
ptk_err client_send_read_coils_req(modbus_connection *con, uint16_t base_coil, uint16_t num_coils);

/* client write functions */
ptk_err client_send_write_holding_register_req(modbus_connection *con, uint16_t register_addr, uint16_t register_value);
ptk_err client_send_write_holding_registers_req(modbus_connection *con, uint16_t base_register, const modbus_register_array_t *register_values);
ptk_err client_send_write_coil_req(modbus_connection *con, uint16_t coil_addr, bool coil_value);
ptk_err client_send_write_coils_req(modbus_connection *con, uint16_t base_coil, const modbus_bool_array_t *coil_values);

ptk_err client_recv_read_read_holding_register_resp(modbus_connection *con, uint16_t *register_value);
ptk_err client_recv_read_read_holding_registers_resp(modbus_connection *con, modbus_register_array_t **register_values);
ptk_err client_recv_read_input_register_resp(modbus_connection *con, uint16_t *register_value);
ptk_err client_recv_read_input_registers_resp(modbus_connection *con, modbus_register_array_t **register_values);
ptk_err client_recv_read_discrete_input_resp(modbus_connection *con, bool *input_value);
ptk_err client_recv_read_discrete_inputs_resp(modbus_connection *con, modbus_bool_array_t **input_values);
ptk_err client_recv_read_coil_resp(modbus_connection *con, bool *coil_value);
ptk_err client_recv_read_coils_resp(modbus_connection *con, modbus_bool_array_t **coil_values);

/* client write response functions */
ptk_err client_recv_write_holding_register_resp(modbus_connection *con);
ptk_err client_recv_write_holding_registers_resp(modbus_connection *con);
ptk_err client_recv_write_coil_resp(modbus_connection *con);
ptk_err client_recv_write_coils_resp(modbus_connection *con);

/* server functions */
modbus_connection *server_accept_connection(modbus_connection *server_connection, ptk_buf_t *buffer);

/* server request receiving functions */
ptk_err server_recv_read_holding_register_req(modbus_connection *conn, uint16_t *register_addr);
ptk_err server_recv_read_holding_registers_req(modbus_connection *conn, uint16_t *base_register, uint16_t *num_registers);
ptk_err server_recv_read_input_register_req(modbus_connection *conn, uint16_t *register_addr);
ptk_err server_recv_read_input_registers_req(modbus_connection *conn, uint16_t *base_register, uint16_t *num_registers);
ptk_err server_recv_read_discrete_input_req(modbus_connection *conn, uint16_t *input_addr);
ptk_err server_recv_read_discrete_inputs_req(modbus_connection *conn, uint16_t *base_input, uint16_t *num_inputs);
ptk_err server_recv_read_coil_req(modbus_connection *conn, uint16_t *coil_addr);
ptk_err server_recv_read_coils_req(modbus_connection *conn, uint16_t *base_coil, uint16_t *num_coils);

/* server write request receiving functions */
ptk_err server_recv_write_holding_register_req(modbus_connection *conn, uint16_t *register_addr, uint16_t *register_value);
ptk_err server_recv_write_holding_registers_req(modbus_connection *conn, uint16_t *base_register, modbus_register_array_t **register_values);
ptk_err server_recv_write_coil_req(modbus_connection *conn, uint16_t *coil_addr, bool *coil_value);
ptk_err server_recv_write_coils_req(modbus_connection *conn, uint16_t *base_coil, modbus_bool_array_t **coil_values);

/* server response sending functions */
ptk_err server_send_read_holding_register_resp(modbus_connection *conn, uint16_t register_value);
ptk_err server_send_read_holding_registers_resp(modbus_connection *conn, const modbus_register_array_t *register_values);
ptk_err server_send_read_input_register_resp(modbus_connection *conn, uint16_t register_value);
ptk_err server_send_read_input_registers_resp(modbus_connection *conn, const modbus_register_array_t *register_values);
ptk_err server_send_read_discrete_input_resp(modbus_connection *conn, bool input_value);
ptk_err server_send_read_discrete_inputs_resp(modbus_connection *conn, const modbus_bool_array_t *input_values);
ptk_err server_send_read_coil_resp(modbus_connection *conn, bool coil_value);
ptk_err server_send_read_coils_resp(modbus_connection *conn, const modbus_bool_array_t *coil_values);

/* server write response sending functions */
ptk_err server_send_write_holding_register_resp(modbus_connection *conn);
ptk_err server_send_write_holding_registers_resp(modbus_connection *conn);
ptk_err server_send_write_coil_resp(modbus_connection *conn);
ptk_err server_send_write_coils_resp(modbus_connection *conn);

/* server error response functions */
ptk_err server_send_exception_resp(modbus_connection *conn, uint8_t function_code, uint8_t exception_code);

#endif // MODBUS_API_H
