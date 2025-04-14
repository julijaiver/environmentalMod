#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "cloud.h"
#include "private.h"



char* cloud_build_request(char* data) {
    // Build the request string
    int size = snprintf(NULL, 0, CLOUD_REQUEST_TEMPLATE, strlen(data), CLOUD_AUTHORIZATION, data);

    char *request = malloc(size + 1);  // +1 for null terminator

    if (!request) {
        // Handle malloc failure
        return NULL;
    }

    snprintf(request, size + 1, CLOUD_REQUEST_TEMPLATE, strlen(data), CLOUD_AUTHORIZATION, data);

return request;
}

// Default time for access token is 1 hour and max time is 12 hours
char* cloud_request_access_token() {
    // TODO: Implement the function
    return NULL;
}

