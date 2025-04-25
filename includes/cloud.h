#ifndef CLOUD_H
#define CLOUD_H
#include "private.h"
// Defines for google cloud pubsub

#define CLOUD_HOST          "pubsub.googleapis.com"
#define CLOUD_CONTENT_TYPE  "application/json"
#define CLOUD_AUTHORIZATION "Bearer"
// Maybe not needed | check modem.c
#define CLOUD_REQUEST_TEMPLATE "POST /v1/projects/" CLOUD_PROJECT_ID "/topics/" CLOUD_TOPIC_ID ":publish HTTP/1.1\r\n" \
                              "Host: " CLOUD_HOST "\r\n" \
                              "Content-Type: " CLOUD_CONTENT_TYPE "\r\n" \
                              "Content-Length: %d\r\n" \
                              "Authorization: " CLOUD_AUTHORIZATION " %s\r\n\r\n" \
                              "%s\r\n"

const char *cloud_request_access_token();

#endif


