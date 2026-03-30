// Includes
#include "jwt.h"
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

#include <zephyr/sys/base64.h>
#include <zephyr/data/jwt.h>


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
    jwt->payload.iat = get_current_time();// clock runs in UTC
    jwt->payload.exp = jwt->payload.iat + 3600;
}

/*
* Constructs the jwt payload to the correct format.
* @param payload of the jwt
* @return Malloced string
*/
char *jwt_construct_payload(struct payload *payload){
    if(payload == NULL){
        printk("Error: jwt payload NULL\n");
        return NULL;
    }
    size_t len = snprintf(NULL, 0, "{\"iss\":\"%s\",\"sub\":\"%s\",\"scope\":\"%s\",\"aud\":\"%s\",\"iat\":%ld,\"exp\":%ld}", payload->iss, payload->sub, payload->scope, payload->audi, (long int)payload->iat, (long int)payload->exp);
    char *result = (char *)k_malloc(len + 1); // Remember to free when constructing
    if(result == NULL){
        printk("Error: failed to allocate memory");
        return NULL;
    }
    snprintf(result, len + 1, "{\"iss\":\"%s\",\"sub\":\"%s\",\"scope\":\"%s\",\"aud\":\"%s\",\"iat\":%ld,\"exp\":%ld}", payload->iss, payload->sub, payload->scope, payload->audi, (long int)payload->iat, (long int)payload->exp);
    return result;
}


char *jwt_build(unsigned char *header, struct payload payload){
    size_t output_len = 0;

    unsigned char *jwt = (char*)k_malloc(2048);

    struct jwt_builder builder = {};
	builder.base = jwt;
	builder.buf = jwt;
	builder.len = 2048;
	builder.overflowed = false;
	builder.pending = 0;


    if(base64_encode(builder.buf, builder.len, &output_len, header, strlen(header))) {
        printk("Header encode failed\n");
        k_free(jwt);
        return NULL;
    }
    builder.buf[output_len] = '.';
    builder.buf += output_len + 1;
    builder.len -= output_len + 1;

    char *constructed_payload = jwt_construct_payload(&payload);
    if(base64_encode(builder.buf, builder.len, &output_len, constructed_payload, strlen(constructed_payload))) {
        printk("Payload encode failed\n");
        k_free(constructed_payload);
        k_free(jwt);
        return NULL;
    }
    builder.buf += output_len;
    builder.len -= output_len;
    
    k_free(constructed_payload);

    // sign jwt
    unsigned char signature[MBEDTLS_PK_SIGNATURE_MAX_SIZE];
    unsigned char hash[32];
    size_t signature_len = 0;
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
        goto cleanup;
    }
   

    if (!mbedtls_pk_can_do(&pk, MBEDTLS_PK_RSA)) { // Check if it's an RSA key context
        printk("ERROR: Parsed key is not recognized as RSA!\n");
        goto cleanup;
    }

    
    mbedtls_sha256(builder.base, builder.buf - builder.base, hash, 0);

    ret = mbedtls_pk_sign(&pk, MBEDTLS_MD_SHA256, hash, 0, signature, MBEDTLS_PK_SIGNATURE_MAX_SIZE, &signature_len, mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret != 0) {
        printk("ERROR: mbedtls_pk_sign FAILED with code %d (0x%x)\n", ret, -ret);
        goto cleanup;
    }
    
    builder.buf[0] = '.';
    builder.buf += 1;
    builder.len -= 1;

    if(base64_encode(builder.buf, builder.len, &output_len, signature, signature_len)) {
        printk("ERROR: failed to encode signature!\n");
        goto cleanup;
    }
    builder.buf[output_len] = 0;
    builder.buf += output_len;
    builder.len -= output_len;

    mbedtls_pk_free(&pk);
    mbedtls_entropy_free(&entropy);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_psa_crypto_free();

    return jwt;

cleanup:
    mbedtls_pk_free(&pk);
    mbedtls_entropy_free(&entropy);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_psa_crypto_free();
    k_free(jwt);
    return NULL;    

#if 0
    unsigned char *der = k_malloc(strlen(private_key_pem));
    unsigned char *s1 = (unsigned char *) strstr((const char *) private_key_pem, "-----BEGIN");
    if(s1 == NULL) {
        k_free(der);
        k_free(jwt);
        return NULL;
    }
    s1+=10; // skip -----BEGIN
    while(*s1 && *s1 != '-') ++s1; // skip until dashes
    while(*s1 =='-') ++s1;
    while(*s1 =='\r') ++s1;
    while(*s1 =='\n') ++s1;
    unsigned char *s2 = (unsigned char *) strstr((const char *) s1, "-----END");
    if (s2 == NULL) {
        k_free(jwt);
        return NULL;
    }
    int keylen = s2 - s1;

    printk("%.10s\n", s1);
    printk("%.10s\n",s2);
    int res =base64_decode(der, strlen(private_key_pem), &output_len, s1, keylen);
    if(res) {
        printk("PEM decode failed %d %d, %d\n", res, (int)output_len, keylen);
        k_free(der);
        k_free(jwt);
        return NULL;
    }

    // this fails - reports that kay is not valid - some field not found 
    res = jwt_sign(&builder, der, output_len);
    if(res) {
        printk("Message sign failed %d\n", res);
        k_free(der);
        k_free(jwt);
        return NULL;
    }
    k_free(der);
    *builder.buf=0;

    return jwt;   
#endif
}
