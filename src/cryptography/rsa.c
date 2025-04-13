#include "rsa.h"
#include "private.h"
#include <zephyr/kernel.h>
#include <string.h>
#include <stdio.h>
#include <mbedtls/pk.h>
#include <mbedtls/md.h>
#include <mbedtls/rsa.h>


//extern static nrf_crypto_rsa_private_key_t private_key;
//extern static uint8_t signature[256];

int rsa_signature(const uint8_t *hash, size_t hash_len, uint8_t *sig, size_t *sig_len){
     int ret = 0;
    
    // Initialize the pk context
    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);

    // Parse the private key from the PEM file
    ret = mbedtls_pk_parse_key(&pk, (const unsigned char *) PRIVATE_KEY, strlen(PRIVATE_KEY) + 1);
    if (ret != 0) {
        char error_buf[100];
        mbedtls_strerror(ret, error_buf, sizeof(error_buf));
        printf("Error: parsing private key: %s\n", error_buf);
        return ret;
    }

    // Check if the key is RSA
    if (!mbedtls_pk_can_do(&pk, MBEDTLS_PK_RSA)) {
        printf("Error: The key is not an RSA key.\n");
        mbedtls_pk_free(&pk);
        return -1;
    }

    mbedtls_rsa_context *rsa = mbedtls_pk_rsa(&pk);
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
    return ret;
}




