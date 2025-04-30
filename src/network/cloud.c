
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// Includes
#include "cloud.h"
#include "private.h"
#include "jwt.h"
#include "modem.h"
#include "base64.h"

// Default time for access token is 1 hour and max time is 12 hours
const char* cloud_request_access_token() {

    jwt_t access_token_jwt;
    modem_status_t modem_ret;

	jwt_init(&access_token_jwt);
	printk("iat: %lld, exp: %lld\n", access_token_jwt.payload.iat, access_token_jwt.payload.exp);
	char *res = jwt_build(JWT_HEADER, access_token_jwt.payload);
    
    modem_ret = start_http_client();
    if(modem_ret != MODEM_SUCCESS){
        stop_http_client();
        return modem_status_to_string(modem_ret);
    }

    char body[1024 * 4];
    size_t body_len;
    snprintf(body, 1024 * 4, "{\"grant_type\": \"urn:ietf:params:oauth:grant-type:jwt-bearer\", \"assertion\": \"%s\"}", res);
    body_len = strlen(body);
    printk("\n%s\n", body);

    modem_ret = send_http_post(ACCESS_TOKEN_URL, ACCESS_TOKEN_CONTENT_TYPE, body, body_len, NULL);
    if(modem_ret != MODEM_SUCCESS){
        char status[1024];
        read_http_response(status);
        printk("RESPONSE: %s\n", status);
        stop_http_client();
        return modem_status_to_string(modem_ret);
    } 


    char *access_token = (char*)malloc(1024 * 2);
    modem_ret = read_http_response(access_token);

    stop_http_client();
    return (const char*)access_token;
}


int cloud_publish(const char *access_token, char *data){
    modem_status_t modem_ret;
    char *tmp_data = "{\"AF:SE:53:CD:66:2D\":{\"temp\":[24.5,25.2,22.5,22.0,22.0,21.7,22.0,22.2,22.0,21.8],\"pressure\":[22.2,21.0,20.4,23.2,20.3,22.3,25.0,23.7,22.6,20.0],\"humidity\":[53.7,71.2,45.8,88.4,36.1,67.9,42.5,59.6,74.3,31.7]},\"B3:9F:2A:7D:44:1E\":{\"temp\":[26.3,24.8,25.5,23.0,24.1,22.9,23.4,24.0,22.7,23.5],\"pressure\":[21.5,23.1,20.9,22.0,24.5,23.9,21.7,25.2,20.6,22.8],\"humidity\":[61.4,48.3,70.1,37.9,79.5,34.2,68.6,55.4,43.7,76.2]}}";
    size_t data_len = strlen(tmp_data);

    char *encoded_data = base64_encrypt(tmp_data, data_len, 0);
    char request_body[2048] = {'\0'};
    snprintf(request_body, 2048, CLOUD_REQUEST_BODY, encoded_data);
    size_t body_len = strlen(request_body);

    modem_ret = start_tcp_socket();
    if(modem_ret != MODEM_SUCCESS){
        stop_tcp_socket();
        return modem_ret;
    } 

    char full_request[1024 * 2] = {'\0'};
    snprintf(full_request, 2048, CLOUD_REQUEST_TEMPLATE, body_len, access_token, request_body);

    printk("%s", full_request);
    size_t req_len = strlen(full_request);
    modem_ret = send_tcp_post_request(full_request, req_len);
    if(modem_ret != MODEM_SUCCESS) {
        stop_tcp_socket();
        return modem_ret;
    }

    stop_tcp_socket();

    return 0;

}