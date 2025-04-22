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
//#include <mbedtls/error.h>



int rsa_signature(const uint8_t *hash, size_t hash_len, uint8_t *sig, size_t *sig_len){
    int ret = 0;
    
    printk("Initializing MBEDTLS\n");
    // Initialize the pk context
    mbedtls_pk_context pk;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;


    mbedtls_pk_init(&pk);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    printk("DONE\n");

    ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, MBEDTLS_CTR_DRBG_MAX_SEED_INPUT);
    if (ret != 0) {
        printk("Error: initializing RNG\n");
        mbedtls_pk_free(&pk);
        mbedtls_ctr_drbg_free(&ctr_drbg);
        mbedtls_entropy_free(&entropy);
        return ret;
    }

    printk("Parsing the key\n");
    // Parse the private key from the PEM file
    ret = mbedtls_pk_parse_key(&pk, GOOGLE_PRIVATE_KEY, strlen(GOOGLE_PRIVATE_KEY), NULL, 0, mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret != 0) {
        printk("Error: parsing private key\n");
        mbedtls_pk_free(&pk);
        mbedtls_ctr_drbg_free(&ctr_drbg);
        mbedtls_entropy_free(&entropy);
        return ret;
    }


    ret = mbedtls_pk_sign(&pk, MBEDTLS_MD_SHA256, hash, hash_len, sig, MBEDTLS_PK_SIGNATURE_MAX_SIZE, sig_len, mbedtls_ctr_drbg_random, &ctr_drbg);
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




