#include "jwt.h"
#include "base64.h"
#include "private.h"

#include <string.h>
#include <stdio.h>
#include <time.h>

#include "mbedtls/sha256.h"
#include "mbedtls/entropy.h"
#include "mbedtls/pk.h"
#include "mbedtls/md.h"
#include "mbedtls/ctr_drbg.h"

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

    
    unsigned char signature[MBEDTLS_PK_SIGNATURE_MAX_SIZE];
    size_t signature_len;
    char *constructed_header_payload = (char*)malloc(1024);
    unsigned char *jwt = (char*)malloc(2048);
    unsigned char hash[32];

    char *constructed_payload = jwt_construct_payload(&payload);
    char *encoded_header = base64_encrypt(header);
    char *encoded_payload = base64_encrypt(constructed_payload);
    char *encoded_signature;
    printk("Payload constructed\n");
    
   
    snprintf(constructed_header_payload, JWT_SIGNATURE_SIZE, "%s.%s", encoded_header, encoded_payload);

    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);
    printk("pk context initialized\n");

    mbedtls_entropy_context entropy;
    mbedtls_entropy_init(&entropy);
    printk("entropy initialized\n");

    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ctr_drbg_init(&ctr_drbg);
    printk("ctr_drbg initialized\n");


    mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, MBEDTLS_CTR_DRBG_MAX_SEED_INPUT);
    printk("random seed generated\n");

   // const unsigned char *private_key = (unsigned char*)GOOGLE_PRIVATE_KEY;

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
    printk("key parsed\n");
    printk("Key type: %d\n", mbedtls_pk_get_type(&pk));

    if (!mbedtls_pk_can_do(&pk, MBEDTLS_PK_RSA)) { // Check if it's an RSA key context
        printk("ERROR: Parsed key is not recognized as RSA!\n");
        // ... handle error, free resources, return ...
        mbedtls_pk_free(&pk);
        mbedtls_entropy_free(&entropy);
        mbedtls_ctr_drbg_free(&ctr_drbg);
        mbedtls_psa_crypto_free();
        free(encoded_header);
        free(encoded_payload);
        free(encoded_signature);
        return NULL;
    }

    
    mbedtls_sha256(constructed_header_payload, strlen((char*)constructed_header_payload), hash, 0);
    printk("sha256 encrypted: %s\n", hash);


    printk("Attempting to sign hash...\n"); // Add this before
    ret = mbedtls_pk_sign(&pk, MBEDTLS_MD_SHA256, hash, 0, signature, MBEDTLS_PK_SIGNATURE_MAX_SIZE, &signature_len, mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret != 0) {
        // *** THIS IS THE MOST IMPORTANT PART NOW ***
        printk("ERROR: mbedtls_pk_sign FAILED with code %d (0x%x)\n", ret, -ret);
        // You MUST handle this error here. Maybe return NULL or an error code.
        // Do not proceed as if signing succeeded!
        // Free resources allocated so far before returning failure.
        mbedtls_pk_free(&pk);
        mbedtls_entropy_free(&entropy);
        mbedtls_ctr_drbg_free(&ctr_drbg);
        mbedtls_psa_crypto_free();
        free(encoded_header);
        free(encoded_payload);
        free(encoded_signature);
        return NULL; // Example: return NULL on failure
    }



  
    encoded_signature = base64_encrypt(signature);
    snprintf(jwt, JWT_SIGNATURE_SIZE, "%s.%s.%s", encoded_header, encoded_payload, encoded_signature);
    printk("jwt constructed\n");

    free(encoded_header);
    free(encoded_payload);
    free(encoded_signature);
    printk("memory freed\n");

    mbedtls_pk_free(&pk);
    printk("pk memory freed\n");

    mbedtls_entropy_free(&entropy);
    printk("entropy memory freed\n");

    mbedtls_ctr_drbg_free(&ctr_drbg);
    printk("ctr_drbg memory freed\n");

    printk("returning\n");
    return jwt;
    
    
}