#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <string.h>
#include "modem.h"
#include "uart.h"
#include "cloud.h"
#include "private.h"

const char *modem_status_to_string(modem_status_t status) {
    switch (status) {
        case MODEM_SUCCESS:					return "MODEM_SUCCESS";
        case MODEM_ERR_AT_COMM:				return "MODEM_ERR_AT_COMM";
        case MODEM_ERR_CREG_SET:			return "MODEM_ERR_CREG_SET";
        case MODEM_ERR_CREG_CHECK:			return "MODEM_ERR_CREG_CHECK";
        case MODEM_ERR_APN_SET:				return "MODEM_ERR_APN_SET";
        case MODEM_ERR_PDP_ACTIVATE:		return "MODEM_ERR_PDP_ACTIVATE";
        case MODEM_ERR_HTTP_INIT:			return "MODEM_ERR_HTTP_INIT";
        case MODEM_ERR_HTTP_URL:			return "MODEM_ERR_HTTP_URL";
        case MODEM_ERR_HTTP_CONTENT_TYPE:	return "MODEM_ERR_HTTP_CONTENT_TYPE";
        case MODEM_ERR_HTTP_DATA_LEN:		return "MODEM_ERR_HTTP_DATA_LEN";
        case MODEM_ERR_HTTP_DATA_SEND:		return "MODEM_ERR_HTTP_DATA_SEND";
        case MODEM_ERR_HTTP_POST:			return "MODEM_ERR_HTTP_POST";
        case MODEM_ERR_HTTP_READ:			return "MODEM_ERR_HTTP_READ";
		case MODEM_ERR_SIM_PIN:				return "MODEM_ERR_SIM_PIN";
		case MODEM_ERR_SSL_CONNECT:			return "MODEM_ERR_SSL_CONNECT";
		case MODEM_ERR_HTTP_SEND_START:		return "MODEM_ERR_HTTP_SEND_START";
		case MODEM_ERR_HTTP_SEND_FAIL:		return "MODEM_ERR_HTTP_SEND_FAIL";
        default:							return "UNKNOWN_ERROR";
    }
}

bool send_at_command(const char *cmd, const char *expected_response, k_timeout_t timeout)
{
	char response[MSG_SIZE];
	
	print_uart(cmd);
	print_uart("\r\n");

	// Wait for response
	while (k_msgq_get(&uart_msgq, &response, timeout) == 0) {
		if (strstr(response, expected_response) != NULL) {
			return true;
		}
		if (strstr(response, "ERROR") != NULL) {
			return false;
		}
	}
	
	return false;
}

modem_status_t initialize_modem(void)
{
	// Test AT communication
	if (!send_at_command("AT", "OK", AT_RESPONSE_TIMEOUT))
		return MODEM_ERR_AT_COMM;

	// Check if SIM PIN is required
	if(send_at_command("AT+CPIN?", "+CPIN=SIM PIN", AT_RESPONSE_TIMEOUT)){
		if(!send_at_command("AT+CPIN=\"1234\"", "OK", AT_RESPONSE_TIMEOUT)){
			return MODEM_ERR_SIM_PIN;
		}
	}

	// Set network registration format
	if (!send_at_command("AT+CREG=2", "OK", AT_RESPONSE_TIMEOUT))
		return MODEM_ERR_CREG_SET;

	// Check network registration
	if (!send_at_command("AT+CREG?", "OK", AT_RESPONSE_TIMEOUT))
		return MODEM_ERR_CREG_CHECK;

	// Set APN (replace with your APN)
	if (!send_at_command("AT+CGDCONT=1,\"IP\",\"internet\"", "OK", AT_RESPONSE_TIMEOUT))
		return MODEM_ERR_APN_SET;

	// Activate PDP context
	if (!send_at_command("AT+CGACT=1,1", "OK", AT_RESPONSE_TIMEOUT)) 
		return MODEM_ERR_PDP_ACTIVATE;

	return MODEM_SUCCESS;
}


modem_status_t send_http_post(const char *token, const char *data){
	char cmd[256];
	char http_msg[1024];
	int content_len = strlen(data);

	//Start SSL connection
	snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"SSL\", \"%s\", 443", CLOUD_HOST);
	if(!send_at_command(cmd, "OK", AT_RESPONSE_TIMEOUT)){
		return MODEM_ERR_SSL_CONNECT;
	}

	// Build full HTTP POST request
	snprintf(http_msg, sizeof(http_msg), CLOUD_REQUEST_TEMPLATE, content_len, token, data);
	snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d", strlen(http_msg));
	if(!send_at_command(cmd, ">", AT_RESPONSE_TIMEOUT)){
		return MODEM_ERR_HTTP_SEND_START;
	}

	// Send HTTP message
	print_uart(http_msg);
	if(!send_at_command("", "SEND OK", AT_RESPONSE_TIMEOUT)){
		return MODEM_ERR_HTTP_SEND_FAIL;
	}

	return MODEM_SUCCESS;
}


/*
modem_status_t send_http_post(const char *url, const char *data)
{
	char cmd[MSG_SIZE];

	// Initialize HTTP service
	if (!send_at_command("AT+HTTPINIT", "OK", AT_RESPONSE_TIMEOUT))
		return MODEM_ERR_HTTP_INIT;

	// Set HTTP URL
	snprintf(cmd, sizeof(cmd), "AT+HTTPPARA=\"URL\",\"%s\"", url);
	if (!send_at_command(cmd, "OK", AT_RESPONSE_TIMEOUT)) 
		return MODEM_ERR_HTTP_URL;

	// Set HTTP content type
	if (!send_at_command("AT+HTTPPARA=\"CONTENT\",\"application/json\"", "OK", AT_RESPONSE_TIMEOUT))
		return MODEM_ERR_HTTP_CONTENT_TYPE;

	// TODO: Implement authorization: bearer token

	// Set HTTP data
	snprintf(cmd, sizeof(cmd), "AT+HTTPDATA=%d,10000", strlen(data));
	if (!send_at_command(cmd, "DOWNLOAD", AT_RESPONSE_TIMEOUT))
		return MODEM_ERR_HTTP_DATA_LEN;

	// Send data
	print_uart(data);
	if (!send_at_command("", "OK", AT_RESPONSE_TIMEOUT))
		return MODEM_ERR_HTTP_DATA_SEND;

	// Send HTTP POST request
	if (!send_at_command("AT+HTTPACTION=1", "OK", AT_RESPONSE_TIMEOUT))
		return MODEM_ERR_HTTP_POST;

	// Wait for response
	if (!send_at_command("AT+HTTPREAD", "OK", AT_RESPONSE_TIMEOUT))
		return MODEM_ERR_HTTP_READ;

	return MODEM_SUCCESS;
}
*/