#ifndef MODEM_H
#define MODEM_H

#include <zephyr/kernel.h>
#include <stdbool.h>

#define AT_RESPONSE_TIMEOUT K_MSEC(5000)



// Status codes for modem operations
typedef enum {
    MODEM_SUCCESS = 0,
    
    // Modem initialization errors
    MODEM_ERR_AT_COMM = -1,
    MODEM_ERR_CREG_SET = -2,
    MODEM_ERR_CREG_CHECK = -3,
    MODEM_ERR_APN_SET = -4,
    MODEM_ERR_PDP_ACTIVATE = -5,
	MODEM_ERR_SIM_PIN = -6,
	
    // HTTP request errors
    MODEM_ERR_HTTP_INIT = -10,
    MODEM_ERR_HTTP_URL = -11,
    MODEM_ERR_HTTP_CONTENT_TYPE = -12,
    MODEM_ERR_HTTP_DATA_LEN = -13,
    MODEM_ERR_HTTP_DATA_SEND = -14,
    MODEM_ERR_HTTP_POST = -15,
    MODEM_ERR_HTTP_READ = -16,
	MODEM_ERR_SSL_CONNECT = -17,
    MODEM_ERR_HTTP_SEND_START = -18,
    MODEM_ERR_HTTP_SEND_FAIL = -19,
    MODEM_ERR_HTTP_TERM = -20,
    MODEM_ERR_HEADER_SET = -21,

    // TCP
    MODEM_ERR_TCP_START = -30,
    MODEM_ERR_TCP_STOP = -31,
    MODEM_ERR_TCP_SEND = -32,
    MODEM_ERR_PDP_START = -33,
    MODEM_ERR_SSL_START = -34,
    MODEM_ERR_PDP_STOP = -35,
    MODEM_ERR_SSL_STOP = -36
} modem_status_t;


const char *modem_status_to_string(modem_status_t status);
bool send_at_command(const char *cmd, const char *expected_response, k_timeout_t timeout);
bool send_at_command_len(const char *cmd, size_t cmd_len, const char *expected_response, k_timeout_t timeout);
modem_status_t initialize_modem(void);
modem_status_t send_http_post(const char *url, const char *content_type, const char *data, size_t data_len, char *headers);
modem_status_t start_http_client(void);
modem_status_t stop_http_client(void);
modem_status_t read_http_response(char *res);
modem_status_t start_tcp_socket(void);
modem_status_t stop_tcp_socket(void);
modem_status_t send_tcp_post_request(const char* request, size_t len);
void modem_get_time(char *buffer, size_t buffer_size);
void modem_shutdown(void);

#endif
