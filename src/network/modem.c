// RTOS
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

// Standard libraries
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Includes
#include "modem.h"
#include "uart.h"
#include "cloud.h"
#include "private.h"
#include "device.h"
#include "cJSON.h"

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
        case MODEM_ERR_HTTP_TERM:           return "MODEM_ERR_HTTP_TERM";
        case MODEM_ERR_HEADER_SET:          return "MODEM_ERR_HEADER_SET";
        case MODEM_ERR_TCP_START:           return "MODEM_ERR_TCP_START";
        case MODEM_ERR_TCP_STOP:            return "MODEM_ERR_TCP_STOP";
        case MODEM_ERR_TCP_SEND:            return "MODEM_ERR_TCP_SEND";
        case MODEM_ERR_PDP_START:           return "MODEM_ERR_PDP_START";
        case MODEM_ERR_SSL_START:           return "MODEM_ERR_SSL_START";
        case MODEM_ERR_PDP_STOP:            return "MODEM_ERR_PDP_STOP";
        case MODEM_ERR_SSL_STOP:            return "MODEM_ERR_SSL_STOP";
        default:							return "UNKNOWN_ERROR";
    }
}

bool send_at_command(const char *cmd, const char *expected_response, k_timeout_t timeout)
{
	return send_at_command_len(cmd, strlen(cmd), expected_response, timeout);
}

bool send_at_command_len(const char *cmd, size_t cmd_len, const char *expected_response, k_timeout_t timeout)
{
	char response[MSG_SIZE];
	
	send_uart(cmd, cmd_len);
	print_uart("\r\n");

	// Wait for response
	while (k_msgq_get(&uart_msgq, &response, timeout) == 0) {
		printk("%s\n", response);
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
	
	if(send_at_command("AT+CPIN?", "+CPIN: SIM PIN", AT_RESPONSE_TIMEOUT)){
		if(!send_at_command("AT+CPIN=\"1234\"", "OK", AT_RESPONSE_TIMEOUT)){
			return MODEM_ERR_SIM_PIN;
		}
	}
    k_msleep(1000);
	/* 
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
	 */

	return MODEM_SUCCESS;
}

modem_status_t start_http_client(){
     if(!send_at_command("AT+HTTPINIT", "OK", AT_RESPONSE_TIMEOUT)) return MODEM_ERR_HTTP_INIT;
     return MODEM_SUCCESS;
}

modem_status_t stop_http_client(){
    if(!send_at_command("AT+HTTPTERM", "OK", AT_RESPONSE_TIMEOUT)) return MODEM_ERR_HTTP_TERM;
    return MODEM_SUCCESS;
}

modem_status_t read_http_response(char *res){
    int ret, response_len = 0;
    char response[1024 * 2];
    char cmd[50];
    char *full_response = (char*)k_malloc(1024 * 2);
    full_response[0] = '\0';

    int i = 0;

    snprintf(cmd, 50, "AT+HTTPREAD?");
    print_uart(cmd);
	print_uart("\r\n");
    while ((ret = k_msgq_get(&uart_msgq, response, AT_RESPONSE_TIMEOUT)) == 0) {
        if (strstr(response, "ERROR") != NULL) {
            printk("ERROR: Modem reported ERROR\n");
            snprintf(res, 8, "ERROR");
            k_free(full_response);
            return MODEM_ERR_HTTP_READ;
        }
        if(strstr(response, "+HTTPREAD:") != NULL){
            if(sscanf(response, "+HTTPREAD: LEN,%d", &response_len) != 1){
                printk("ERROR: can't parse response length\n");
            }

        }
        i++;
    }

    snprintf(cmd, 256, "AT+HTTPREAD=0,%d", response_len);
    if(!send_at_command(cmd, "OK", AT_RESPONSE_TIMEOUT)) return MODEM_ERR_HTTP_READ;
    
    i = 0;
    while((ret = k_msgq_get(&uart_msgq, response, AT_RESPONSE_TIMEOUT)) == 0){
        if (strstr(response, "ERROR") != NULL) {
            printk("ERROR: Modem reported ERROR\n");
            snprintf(res, 8, "ERROR");
            k_free(full_response);
            return MODEM_ERR_HTTP_READ;
        }
        if(strstr(response, "+HTTPREAD:") == NULL){ 
            strcat(full_response, response);
        }
        i++;
    }
    
    cJSON *response_json = cJSON_Parse(deleteChar(full_response, '\n'));
    k_free(full_response);
    
    if(response_json == NULL) return MODEM_ERR_HTTP_READ;
    cJSON *token = cJSON_GetObjectItemCaseSensitive(response_json, "access_token");
    snprintf(res, 1024 * 2, "%s", token->valuestring);

    cJSON_Delete(response_json);

    return MODEM_SUCCESS;
}

/*
* @param url url to send request 
* @param content_type ex. application/json, x-www-form-urlencoded etc... 
* @param data body of POST request
* @param data_len length of the body
* @return Status code for modem
*/
modem_status_t send_http_post(const char *url, const char *content_type, const char *data, size_t data_len, char *headers){
	char cmd[1024 * 2];
    size_t cmd_len = sizeof(cmd);
    if(data_len > cmd_len - 64) return MODEM_ERR_HTTP_DATA_LEN;
 
    snprintf(cmd, cmd_len, "AT+HTTPPARA=\"URL\", %s", url);
    if(!send_at_command(cmd, "OK", AT_RESPONSE_TIMEOUT)) return MODEM_ERR_HTTP_URL;

    snprintf(cmd, cmd_len, "AT+HTTPPARA=\"CONTENT\", %s", content_type);
    if(!send_at_command(cmd, "OK", AT_RESPONSE_TIMEOUT)) return MODEM_ERR_HTTP_CONTENT_TYPE;

    if(headers != NULL){
        snprintf(cmd, cmd_len, "AT+HTTPPARA=\"USERDATA\",%s", headers);
        if(!send_at_command(cmd, "OK", AT_RESPONSE_TIMEOUT)) return MODEM_ERR_HEADER_SET;
    }

    snprintf(cmd, cmd_len, "AT+HTTPDATA=%u, 10000", data_len);
    if(send_at_command(cmd, "DOWNLOAD", AT_RESPONSE_TIMEOUT)){
        snprintf(cmd, cmd_len, "%s", data);
        if(!send_at_command(cmd, "OK", AT_RESPONSE_TIMEOUT)) return MODEM_ERR_HTTP_DATA_SEND;
    } else {
        return MODEM_ERR_HTTP_DATA_LEN;
    }
    if(!send_at_command("AT+HTTPACTION=1", "200", AT_RESPONSE_TIMEOUT)) return MODEM_ERR_HTTP_SEND_FAIL;

	return MODEM_SUCCESS;
}

void modem_get_time(char *buffer, size_t buffer_size){
	char cmd[256];
    char response[MSG_SIZE];
    int time_extracted = 0; 
    int ok_received = 0;    
    int ret;

    if (buffer == NULL || buffer_size == 0) {
        return; 
    }
    buffer[0] = '\0'; 

    snprintf(cmd, sizeof(cmd), "AT+CCLK?");
    print_uart(cmd);
    print_uart("\r\n");

    while ((ret = k_msgq_get(&uart_msgq, response, AT_RESPONSE_TIMEOUT)) == 0) {
        
        printk("%s\n", response);

        if (strstr(response, "ERROR") != NULL) {
            printk("ERROR: Modem reported ERROR\n");
            snprintf(buffer, buffer_size, "ERROR");
            return;
        }
        char *start = strstr(response, "+CCLK: ");

        if (start) {
            char *quote1 = strchr(start, '\"');

            if (quote1) {
                char *quote2 = strchr(quote1 + 1, '\"');

                if (quote2) {
                    size_t time_len = quote2 - (quote1 + 1);
                     
                    if (time_len < buffer_size) {
                        memcpy(buffer, quote1 + 1, time_len);
                        buffer[time_len] = '\0'; 
                        time_extracted = 1;
                    } else {
                        printk("ERROR: Extracted time string too long for provided buffer (%u >= %u)\n", (unsigned int)time_len, (unsigned int)buffer_size);
                        snprintf(buffer, buffer_size, "ERROR");
                        return;
                    }
                } else {
                    printk("ERROR: Found opening quote in +CCLK but no closing quote.\n");
                    snprintf(buffer, buffer_size, "ERROR");
                }
            } else {
                printk("ERROR: Found +CCLK but no opening quote.\n");
                snprintf(buffer, buffer_size, "ERROR");
            }
        }

        if (strstr(response, "OK") != NULL) {
            ok_received = 1;
            break;
        }
    } 

    // Check results after loop finishes 
    if (ret != 0) {
        printk("ERROR: k_msgq_get timed out or failed (%d)\n", ret);
        snprintf(buffer, buffer_size, "ERROR");
        return; 
    }

    if (time_extracted && ok_received) {
        return;
    } else if (time_extracted && !ok_received) {
        printk("WARN: Extracted time but did not receive OK confirmation.\n");
        snprintf(buffer, buffer_size, "ERROR");
        return;
    } else if (!time_extracted && ok_received) {
        printk("ERROR: Received OK but failed to extract time.\n");
        snprintf(buffer, buffer_size, "ERROR");
        return;
    } else {
        printk("ERROR: Command sequence finished without extracting time or receiving OK.\n");
        snprintf(buffer, buffer_size, "ERROR");
        return; 
    }
}

void modem_shutdown(void){
    send_at_command("AT+CPOF", "OK", AT_RESPONSE_TIMEOUT);
}

modem_status_t start_tcp_socket(void){
    if(!send_at_command("AT+NETOPEN?", "+NETOPEN: 1", AT_RESPONSE_TIMEOUT)){
        if(!send_at_command("AT+NETOPEN", "OK", AT_RESPONSE_TIMEOUT)) return MODEM_ERR_PDP_START;
    }

    if(!send_at_command("AT+CSSLCFG=\"sslversion\", 0,3", "OK", AT_RESPONSE_TIMEOUT)) return MODEM_ERR_SSL_START;
    if(!send_at_command("AT+CSSLCFG=\"authmode\",0,0", "OK", AT_RESPONSE_TIMEOUT)) return MODEM_ERR_SSL_START;
    if(!send_at_command("AT+CCHSET=1", "OK", AT_RESPONSE_TIMEOUT)) return MODEM_ERR_TCP_START;
    if(!send_at_command("AT+CCHSTART", "OK", AT_RESPONSE_TIMEOUT)) return MODEM_ERR_TCP_START;
    if(!send_at_command("AT+CCHSSLCFG=0,0", "OK", AT_RESPONSE_TIMEOUT)) return MODEM_ERR_TCP_START;
    if(!send_at_command("AT+CCHOPEN=0,\"pubsub.googleapis.com\",443,2", "OK", AT_RESPONSE_TIMEOUT)) return MODEM_ERR_TCP_START;
    if(!send_at_command("AT+CCHOPEN?", "OK", AT_RESPONSE_TIMEOUT)) return MODEM_ERR_TCP_START;
    //if(!send_at_command("AT+CCHSETOPT=0,2,0", "OK", AT_RESPONSE_TIMEOUT)) return MODEM_ERR_TCP_START;

    return MODEM_SUCCESS;
}

modem_status_t stop_tcp_socket(void){
    send_at_command("AT+CCHCLOSE=0", "OK", AT_RESPONSE_TIMEOUT);
    send_at_command("AT+CCHSTOP", "OK", AT_RESPONSE_TIMEOUT);
    send_at_command("AT+NETCLOSE", "OK", AT_RESPONSE_TIMEOUT);
    return MODEM_SUCCESS;
}

modem_status_t send_tcp_post_request(const char* request, size_t len){
    char cmd[64];

    snprintf(cmd, 64, "AT+CCHSEND=0, %u", len);
    if(!send_at_command(cmd, ">", AT_RESPONSE_TIMEOUT));
    
    k_msleep(100);
    send_at_command_len(request, len, "OK", AT_RESPONSE_TIMEOUT);

    return MODEM_SUCCESS;
}