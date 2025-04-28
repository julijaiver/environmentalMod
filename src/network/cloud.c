
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// Includes
#include "cloud.h"
#include "private.h"
#include "jwt.h"
#include "modem.h"

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

    char body[1024 * 10];
    size_t body_len;
    snprintf(body, 1024 * 10, "{\"grant_type\": \"urn:ietf:params:oauth:grant-type:jwt-bearer\", \"assertion\": \"%s\"}", res);
    body_len = strlen(body);
    printk("\n%s\n", body);

    modem_ret = send_http_post(ACCESS_TOKEN_URL, ACCESS_TOKEN_CONTENT_TYPE, body, body_len);
    if(modem_ret != MODEM_SUCCESS){
        char status[1024];
        read_http_response(status);
        printk("RESPONSE: %s\n", status);
        stop_http_client();
        return modem_status_to_string(modem_ret);
    } 


    char *access_token = (char*)malloc(1024 * 8);
    modem_ret = read_http_response(access_token);

    stop_http_client();
    return (const char*)access_token;
}
