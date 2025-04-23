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
    MODEM_ERR_HTTP_SEND_FAIL = -19
} modem_status_t;


const char *modem_status_to_string(modem_status_t status);
bool send_at_command(const char *cmd, const char *expected_response, k_timeout_t timeout);
modem_status_t initialize_modem(void);
modem_status_t send_http_post(const char *url, const char *data);
void modem_get_time(char *buffer, size_t buffer_size);


#endif
