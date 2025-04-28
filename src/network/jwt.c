// Includes
#include "jwt.h"
#include "base64.h"
#include "private.h"
#include "realtime.h"

// Standard libraries
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

// Cryptography
#include "mbedtls/sha256.h"
#include "mbedtls/entropy.h"
#include "mbedtls/pk.h"
#include "mbedtls/md.h"
#include "mbedtls/ctr_drbg.h"

// RTOS
#include <zephyr/kernel.h>



void jwt_init(jwt_t *jwt){
    if(jwt == NULL){
        printk("Error: NULL pointer passed to jwt_init\n");
        return;
    }

    jwt->header = JWT_HEADER;

    jwt->payload.audi = JWT_AUDIENCE;
    jwt->payload.scope = JWT_SCOPE;
    jwt->payload.iss = JWT_ISSUER;
    jwt->payload.sub = JWT_SUBJECT;
    jwt->payload.iat = get_current_time() - (3600 * 3); // Adjust to UCT
    jwt->payload.exp = jwt->payload.iat + 3600;
}

char *jwt_construct_payload(struct payload *payload){
    if(payload == NULL){
        printk("Error: jwt payload NULL\n");
        return NULL;
    }
    size_t len = snprintf(NULL, 0, "{\"iss\":\"%s\",\"sub\":\"%s\",\"scope\":\"%s\",\"aud\":\"%s\",\"iat\":%ld,\"exp\":%ld}", payload->iss, payload->sub, payload->scope, payload->audi, (long int)payload->iat, (long int)payload->exp);
    char *result = (char *)malloc(len + 1); // Remember to free when constructing
    if(result == NULL){
        printk("Error: failed to allocate memory");
        return NULL;
    }
    snprintf(result, len + 1, "{\"iss\":\"%s\",\"sub\":\"%s\",\"scope\":\"%s\",\"aud\":\"%s\",\"iat\":%ld,\"exp\":%ld}", payload->iss, payload->sub, payload->scope, payload->audi, (long int)payload->iat, (long int)payload->exp);
    return result;
}

char *jwt_build(unsigned char *header, struct payload payload){
    
    unsigned char signature[MBEDTLS_PK_SIGNATURE_MAX_SIZE];
    size_t signature_len, header_len, payload_len;

    char *constructed_header_payload = (char*)malloc(1024);
    unsigned char *jwt = (char*)malloc(2048);
    unsigned char hash[32];

    char *constructed_payload = jwt_construct_payload(&payload);
    header_len = strlen((char*)header);
    char *encoded_header = base64_encrypt(header, header_len, 0);
    payload_len = strlen(constructed_payload);
    char *encoded_payload = base64_encrypt(constructed_payload, payload_len, 0);
    char *encoded_signature;
    
   
    snprintf(constructed_header_payload, JWT_SIGNATURE_SIZE, "%s.%s", encoded_header, encoded_payload);

    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);

    mbedtls_entropy_context entropy;
    mbedtls_entropy_init(&entropy);

    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ctr_drbg_init(&ctr_drbg);


    mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, MBEDTLS_CTR_DRBG_MAX_SEED_INPUT);


    int ret = mbedtls_pk_parse_key(&pk, private_key_pem, (strlen((char*) private_key_pem) + 1), NULL, 0, mbedtls_ctr_drbg_random, &ctr_drbg);
    if(ret != 0){
        printk("ERROR: %d\n", ret);
        mbedtls_pk_free(&pk);
        mbedtls_entropy_free(&entropy);
        mbedtls_ctr_drbg_free(&ctr_drbg);
        mbedtls_psa_crypto_free();
        free(constructed_header_payload);
        free(constructed_payload);
        return NULL;
    }
   

    if (!mbedtls_pk_can_do(&pk, MBEDTLS_PK_RSA)) { // Check if it's an RSA key context
        printk("ERROR: Parsed key is not recognized as RSA!\n");
        // ... handle error, free resources, return ...
        mbedtls_pk_free(&pk);
        mbedtls_entropy_free(&entropy);
        mbedtls_ctr_drbg_free(&ctr_drbg);
        mbedtls_psa_crypto_free();
        free(encoded_header);
        free(encoded_payload);
        return NULL;
    }

    
    mbedtls_sha256(constructed_header_payload, strlen((char*)constructed_header_payload), hash, 0);
   
    
    ret = mbedtls_pk_sign(&pk, MBEDTLS_MD_SHA256, hash, 0, signature, MBEDTLS_PK_SIGNATURE_MAX_SIZE, &signature_len, mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret != 0) {
        printk("ERROR: mbedtls_pk_sign FAILED with code %d (0x%x)\n", ret, -ret);
        
        // Free resources allocated so far before returning failure.
        mbedtls_pk_free(&pk);
        mbedtls_entropy_free(&entropy);
        mbedtls_ctr_drbg_free(&ctr_drbg);
        mbedtls_psa_crypto_free();
        free(encoded_header);
        free(encoded_payload);
        return NULL;
    }
    
    encoded_signature = base64_encrypt(signature, signature_len, 0);
    snprintf(jwt, JWT_SIGNATURE_SIZE, "%s.%s.%s", encoded_header, encoded_payload, encoded_signature);

    free(encoded_header);
    free(encoded_payload);
    free(encoded_signature);

    mbedtls_pk_free(&pk);
    mbedtls_entropy_free(&entropy);
    mbedtls_ctr_drbg_free(&ctr_drbg);

    return jwt;
    
    
}