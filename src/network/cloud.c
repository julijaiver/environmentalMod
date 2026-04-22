#include <zephyr/sys/base64.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// Includes
#include "cloud.h"
#include "private.h"
#include "jwt.h"
#include "modem.h"


/*
* Default time for access token is 1 hour and max time is 12 hours
* @return Malloced string
*/
const char* cloud_request_access_token(void) {

    jwt_t access_token_jwt;
    modem_status_t modem_ret;
    char *token_buffer;
    size_t body_len;

	jwt_init(&access_token_jwt);

	char *jwt = jwt_build(JWT_HEADER, access_token_jwt.payload);
    if(jwt == NULL) return NULL;

    token_buffer = (char*)k_malloc(ACCESS_TOKEN_BODY);
    snprintf(token_buffer, ACCESS_TOKEN_BODY, "{\"grant_type\": \"urn:ietf:params:oauth:grant-type:jwt-bearer\", \"assertion\": \"%s\"}", jwt);
    body_len = strlen(token_buffer);
    k_free(jwt);

    modem_ret = start_http_client();
    if(modem_ret != MODEM_SUCCESS){
        stop_http_client();
        k_free(token_buffer);
        return NULL;
    }
    //printk("Token req: %u\n", body_len);
    modem_ret = send_http_post(ACCESS_TOKEN_URL, ACCESS_TOKEN_CONTENT_TYPE, token_buffer, body_len, NULL);
    if(modem_ret != MODEM_SUCCESS){
        stop_http_client();
        k_free(token_buffer);
        return NULL;
    } 

    if(read_http_response(token_buffer) != MODEM_SUCCESS) {
        k_free(token_buffer);
        token_buffer = NULL;
    }

    stop_http_client();

    return token_buffer;
}

/*
* @param access_token access token returned from google OAuth2.0
* @param data unformatted json string of sensor data
* @return 0, modem_status_t on failure   
*/
int cloud_publish(const char *access_token, const char *data){
    modem_status_t modem_ret;
    //char *tmp_data = "{\"AF:SE:53:CD:66:2D\":{\"temp\":[24.5,25.2,22.5,22.0,22.0,21.7,22.0,22.2,22.0,21.8,23.1,22.3,23.9,24.4,25.1,24.6,23.5,22.9,22.8,21.6,21.9,22.3,23.0,24.2,25.4,24.0,23.3,22.1,21.8,21.5],\"pressure\":[22.2,21.0,20.4,23.2,20.3,22.3,25.0,23.7,22.6,20.0,24.4,25.1,24.3,23.8,22.2,20.1,21.5,22.8,23.9,24.6,22.1,23.3,21.7,24.8,23.0,21.2,20.6,22.7,24.5,25.2],\"humidity\":[53.7,71.2,45.8,88.4,36.1,67.9,42.5,59.6,74.3,31.7,60.2,62.5,44.1,38.6,70.7,55.9,61.3,66.1,49.4,47.0,52.6,50.3,69.0,64.4,48.9,36.5,72.1,40.0,58.3,60.8]},\"B3:9F:2A:7D:44:1E\":{\"temp\":[26.3,24.8,25.5,23.0,24.1,22.9,23.4,24.0,22.7,23.5,23.2,22.6,24.9,25.7,26.1,25.4,24.8,24.0,23.6,22.8,23.1,22.4,22.9,24.3,25.6,23.9,24.5,22.2,21.8,23.7],\"pressure\":[21.5,23.1,20.9,22.0,24.5,23.9,21.7,25.2,20.6,22.8,24.1,23.0,22.4,21.1,23.3,25.0,24.6,21.5,23.7,24.2,22.5,22.1,21.3,20.8,24.4,23.5,22.7,23.1,21.8,22.9],\"humidity\":[61.4,48.3,70.1,37.9,79.5,34.2,68.6,55.4,43.7,76.2,60.5,62.7,65.1,69.3,58.4,66.0,70.8,71.6,67.2,73.4,59.1,60.9,56.5,54.8,68.3,72.4,53.1,50.2,63.6,66.8]},\"DE:AD:BE:EF:00:01\":{\"temp\":[20.2,19.8,20.0,21.0,20.5,20.3,20.7,21.1,20.9,20.4,20.0,21.2,21.5,21.7,20.6,20.3,19.9,19.5,20.1,21.3,21.0,20.8,20.6,20.2,19.7,19.3,20.4,21.4,21.6,20.7],\"pressure\":[19.5,20.0,20.3,21.0,20.8,20.5,21.3,20.7,20.6,21.1,19.8,20.4,20.9,21.2,20.6,20.0,19.9,19.5,20.2,21.1,21.4,20.7,20.3,20.6,19.7,20.9,21.0,20.1,20.8,21.3],\"humidity\":[50.0,51.2,52.3,53.4,50.1,48.7,47.9,49.5,50.8,52.0,53.1,54.2,55.3,56.4,57.5,58.6,59.7,60.8,61.9,63.0,64.1,65.2,66.3,67.4,68.5,69.6,70.7,71.8,72.9,74.0]},\"C0:FF:EE:C0:DE:77\":{\"temp\":[28.4,28.0,27.5,27.8,28.1,28.3,28.2,27.9,27.7,28.0,27.6,28.1,27.8,28.3,28.4,27.9,27.5,28.0,28.2,28.3,28.1,27.7,28.4,28.0,27.8,28.3,28.1,28.2,27.9,28.0],\"pressure\":[24.5,24.7,24.6,24.3,24.4,24.2,24.0,24.1,24.3,24.6,24.7,24.5,24.8,24.2,24.0,24.3,24.1,24.7,24.6,24.5,24.3,24.0,24.4,24.5,24.2,24.3,24.7,24.1,24.0,24.3],\"humidity\":[45.5,46.2,47.3,48.1,46.7,45.0,44.5,46.0,45.2,46.8,47.9,48.5,49.0,49.8,50.3,51.0,52.1,53.3,54.5,55.0,55.7,56.3,57.0,57.6,58.4,59.1,59.9,60.5,61.2,62.0]}}";
    char *full_request = k_calloc(FULL_REQUEST_SIZE, sizeof(char));
    char *request_body = k_calloc(REQUEST_BODY_SIZE, sizeof(char));


    // first use full request as tmp during base64 encoding
    size_t data_len = 0;
    char *encoded_data = full_request;
    if(base64_encode(encoded_data, FULL_REQUEST_SIZE, &data_len, data, strlen(data))) {
        k_free(full_request);
        k_free(request_body);
        return -999; // out of memory - should never happen
    }
    encoded_data[data_len] = 0; // nul-terminate
    
    // form request body from encoded data
    snprintf(request_body, REQUEST_BODY_SIZE, CLOUD_REQUEST_BODY, encoded_data);
    size_t body_len = strlen(request_body);
    
    // form full request from requset body and access token
    snprintf(full_request, FULL_REQUEST_SIZE, CLOUD_REQUEST_TEMPLATE, body_len, access_token, request_body);
    size_t req_len = strlen(full_request);
    
    k_free(request_body);

    modem_ret = start_tcp_socket();
    if(modem_ret != MODEM_SUCCESS){
        stop_tcp_socket();
        k_free(full_request);
        return modem_ret;
    } 
    for(int i = 0; i < req_len; i += 2048){
        size_t remain = req_len - i;
        char *chunk = full_request + i;
        modem_ret = send_tcp_post_request(chunk, remain >= 2048 ? 2048 : remain);
    }
    
    k_free(full_request);
    
    stop_tcp_socket();
    return modem_ret;
}