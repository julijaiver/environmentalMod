
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
    
    size_t res_len = sizeof(sizeof(&res) / sizeof(res[0]));
    modem_ret = send_http_post(ACCESS_TOKEN_URL, ACCESS_TOKEN_CONTENT_TYPE, res, res_len);
    if(modem_ret != MODEM_SUCCESS) return modem_status_to_string(modem_ret);


    char *access_token = (char*)malloc(1024 * 8);
    modem_ret = read_http_response(access_token);

	printk("Signed: %s\n", res);

    return (const char*)access_token;
}
