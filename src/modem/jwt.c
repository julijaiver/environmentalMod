#include "jwt.h"
#include "base64.h"
#include "sha256.h"
#include "rsa.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "mbedtls/pk.h"
#include "mbedtls/md.h"
#include <zephyr/kernel.h>


void jwt_init(jwt_t *jwt){
    if(jwt == NULL){
        printk("Error: NULL pointer passed to jwt_init\n");
        return;
    }

    jwt->header = JWT_HEADER;

    jwt->payload.audi = JWT_AUDIENCE;
    jwt->payload.iss = JWT_ISSUER;
    jwt->payload.sub = JWT_SUBJECT;
    jwt->payload.iat = 10; // TODO: get time
    jwt->payload.exp = jwt->payload.iat + 3600;

    jwt->signature = "signature";
}

char *jwt_construct_payload(struct payload *payload){
    if(payload == NULL){
        printk("Error: jwt payload NULL\n");
        return NULL;
    }
    size_t len = snprintf(NULL, 0, "{\"iss\": \"%s\", \"sub\": \"%s\", \"aud\": \"%s\", \"iat\": %u, \"exp\": %u}", payload->iss, payload->sub, payload->audi, payload->iat, payload->exp);
    char *result = (char *)malloc(len + 1); // Remember to free when constructing
    if(result == NULL){
        printk("Error: failed to allocate memory");
        return NULL;
    }
    snprintf(result, len + 1, "{\"iss\": \"%s\", \"sub\": \"%s\", \"aud\": \"%s\", \"iat\": %u, \"exp\": %u}", payload->iss, payload->sub, payload->audi, payload->iat, payload->exp);
    return result;
}

char *jwt_build(char *header, struct payload payload){
    // TODO: build in format b64.b64.b64(rs256)
    char signing_input[2048];
    uint8_t signature[256];
    size_t sig_len;

    snprintf(signing_input, sizeof(signing_input), "%s.%s", base64_encrypt(header), base64_encrypt(jwt_construct_payload(&payload)));
    char *sha256 = SHA256(signing_input);

    
    int ret = rsa_signature(sha256, (sizeof(sha256) / sizeof(sha256[0])), signature, &sig_len);
    if (ret != 0) {
        printk("Failed to parse RSA private key.\n");
        return ret;
    } else {
        printk("RSA Private Key successfully parsed.\n");
        
    }
    


    return signature;
}