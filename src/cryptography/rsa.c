#include "rsa.h"
#include "private.h"
#include <zephyr/kernel.h>
#include <string.h>
#include <stdio.h>
#include <mbedtls/pk.h>
#include <mbedtls/md.h>
#include <mbedtls/rsa.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>



int rsa_signature(const uint8_t *hash, size_t hash_len, uint8_t *sig, size_t *sig_len){
    int ret = 0;
    char error_buf[100];
    // Initialize the pk context
    mbedtls_pk_context pk;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;


    mbedtls_pk_init(&pk);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);


    ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, 0);
    if (ret != 0) {
        mbedtls_strerror(ret, error_buf, sizeof(error_buf));
        printk("Error: initializing RNG: %s\n", error_buf);
        mbedtls_pk_free(&pk);
        mbedtls_ctr_drbg_free(&ctr_drbg);
        mbedtls_entropy_free(&entropy);
        return ret;
    }
    // Parse the private key from the PEM file
    ret = mbedtls_pk_parse_key(&pk, GOOGLE_PRIVATE_KEY, strlen(GOOGLE_PRIVATE_KEY), NULL, 0, mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret != 0) {
        
        mbedtls_strerror(ret, error_buf, sizeof(error_buf));
        printk("Error: parsing private key: %s\n", error_buf);
        mbedtls_pk_free(&pk);
        mbedtls_ctr_drbg_free(&ctr_drbg);
        mbedtls_entropy_free(&entropy);
        return ret;
    }

    // Check if the key is RSA
    if (!mbedtls_pk_can_do(&pk, MBEDTLS_PK_RSA)) {
        printf("Error: The key is not an RSA key.\n");
        mbedtls_pk_free(&pk);
        mbedtls_ctr_drbg_free(&ctr_drbg);
        mbedtls_entropy_free(&entropy);
        return -1;
    }

    mbedtls_rsa_context *rsa = mbedtls_pk_rsa(pk);
    mbedtls_rsa_set_padding(rsa, MBEDTLS_RSA_PKCS_V15, 0); // PKCS#1 v1.5

    ret = mbedtls_rsa_pkcs1_sign(rsa,
                                 NULL, NULL, // No RNG needed for deterministic signing
                                 MBEDTLS_MD_SHA256,
                                 hash_len,
                                 hash,
                                 sig);
    if (ret != 0) {
        printf("Error: RSA sign error: -0x%04X\n", -ret);
    } else {
        *sig_len = mbedtls_pk_get_len(&pk);
    }

    mbedtls_pk_free(&pk);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);

    return ret;
}




