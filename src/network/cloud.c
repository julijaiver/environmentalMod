
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

/*
* Default time for access token is 1 hour and max time is 12 hours
* @return Malloced string
*/
const char* cloud_request_access_token(void) {

    jwt_t access_token_jwt;
    modem_status_t modem_ret;

	jwt_init(&access_token_jwt);

	char *res = jwt_build(JWT_HEADER, access_token_jwt.payload);
    modem_ret = start_http_client();
    if(modem_ret != MODEM_SUCCESS){
        stop_http_client();
        k_free(res);
        return modem_status_to_string(modem_ret);
    }

    char body[ACCESS_TOKEN_BODY];
    size_t body_len;
    snprintf(body, ACCESS_TOKEN_BODY, "{\"grant_type\": \"urn:ietf:params:oauth:grant-type:jwt-bearer\", \"assertion\": \"%s\"}", res);
    body_len = strlen(body);
    k_free(res);

    modem_ret = send_http_post(ACCESS_TOKEN_URL, ACCESS_TOKEN_CONTENT_TYPE, body, body_len, NULL);
    if(modem_ret != MODEM_SUCCESS){
        stop_http_client();
        return modem_status_to_string(modem_ret);
    } 

    char *access_token = (char*)k_malloc(ACCESS_TOKEN_SIZE);
    modem_ret = read_http_response(access_token);

    stop_http_client();
    return (const char*)access_token;
}

/*
* @param access_token access token returned from google OAuth2.0
* @param data unformatted json string of sensor data
* @return 0, modem_status_t on failure   
*/
int cloud_publish(const char *access_token, const char *data){
    modem_status_t modem_ret;
   // char *tmp_data = "{\"AF:SE:53:CD:66:2D\":{\"temp\":[24.5,25.2,22.5,22.0,22.0,21.7,22.0,22.2,22.0,21.8],\"pressure\":[22.2,21.0,20.4,23.2,20.3,22.3,25.0,23.7,22.6,20.0],\"humidity\":[53.7,71.2,45.8,88.4,36.1,67.9,42.5,59.6,74.3,31.7]},\"B3:9F:2A:7D:44:1E\":{\"temp\":[26.3,24.8,25.5,23.0,24.1,22.9,23.4,24.0,22.7,23.5],\"pressure\":[21.5,23.1,20.9,22.0,24.5,23.9,21.7,25.2,20.6,22.8],\"humidity\":[61.4,48.3,70.1,37.9,79.5,34.2,68.6,55.4,43.7,76.2]}}";
    char *full_request = k_calloc(FULL_REQUEST_SIZE, sizeof(char));
    char *request_body = k_calloc(REQUEST_BODY_SIZE, sizeof(char));

    size_t data_len = strlen(data);
    char *encoded_data = base64_encrypt(data, data_len, 0);
    
    snprintf(request_body, REQUEST_BODY_SIZE, CLOUD_REQUEST_BODY, encoded_data);
    size_t body_len = strlen(request_body);
    k_free(encoded_data);
    
    snprintf(full_request, FULL_REQUEST_SIZE, CLOUD_REQUEST_TEMPLATE, body_len, access_token, request_body);
    size_t req_len = strlen(full_request);
    
    k_free(request_body);

    modem_ret = start_tcp_socket();
    if(modem_ret != MODEM_SUCCESS){
        stop_tcp_socket();
        k_free(full_request);
        return modem_ret;
    } 
    
    modem_ret = send_tcp_post_request(full_request, req_len);
    k_free(full_request);
    if(modem_ret != MODEM_SUCCESS) {
        stop_tcp_socket();
        return modem_ret;
    }

    stop_tcp_socket();
    return 0;
}