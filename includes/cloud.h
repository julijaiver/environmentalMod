#ifndef CLOUD_H
#define CLOUD_H
#include "private.h"

// Max size for pub message is 10MB device has 256KB RAM
#define FULL_REQUEST_SIZE       1024 * 15
#define REQUEST_BODY_SIZE       1024 * 12
#define ACCESS_TOKEN_SIZE       1024

// Access Token
#define ACCESS_TOKEN_BODY       1024 * 4


// Templates
#define CLOUD_REQUEST_TEMPLATE "POST /v1/projects/" GOOGLE_CLOUD_PROJECT_ID "/topics/" GOOGLE_CLOUD_TOPIC_ID ":publish HTTP/1.1\r\n" \
                              "Host: " CLOUD_HOST "\r\n" \
                              "Content-Type: " CLOUD_CONTENT_TYPE "\r\n" \
                              "Content-Length: %u\r\n" \
                              "Authorization: " CLOUD_AUTHORIZATION " %s\r\n\r\n" \
                              "%s\r\n"

#define CLOUD_REQUEST_BODY "{\"messages\":[{\"data\":\"%s\"}]}"
const char *cloud_request_access_token(void);
int cloud_publish(const char *access_token, const char *data);

#endif


