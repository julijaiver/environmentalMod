#ifndef CLOUD_H
#define CLOUD_H

// Defines for google cloud pubsub
#define CLOUD_PROJECT_ID    "prj-mtp-jaak-leht-uf"
#define CLOUD_TOPIC_ID      "environmental-thesis"
#define CLOUD_HOST          "pubsub.googleapis.com"
#define CLOUD_CONTENT_TYPE  "application/json"
#define CLOUD_AUTHORIZATION "Bearer "
// Maybe not needed | check modem.c
#define CLOUD_REQUEST_TEMPLATE "POST /v1/projects/" CLOUD_PROJECT_ID "/topics/" CLOUD_TOPIC_ID ":publish HTTP/1.1\r\n" \
                              "Host: " CLOUD_HOST "\r\n" \
                              "Content-Type: " CLOUD_CONTENT_TYPE "\r\n" \
                              "Content-Length: %d\r\n" \
                              "Authorization: " CLOUD_AUTHORIZATION "%s\r\n\r\n" \
                              "%s\r\n"


// "{\"messages\": [{\"data\": \"%s\"}]}"

char* cloud_build_request(char* data);
char* cloud_request_access_token();

#endif


