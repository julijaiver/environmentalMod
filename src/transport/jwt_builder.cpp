#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(jwt_builder);

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/base64.h>

#include "mbedtls/sha256.h"
#include "mbedtls/pk.h"
#include "mbedtls/md.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"

#include "transport/jwt_builder.hpp"
#include "utils/system_clock.hpp"

extern "C" {
#include "private.h"
}

char *JwtBuilder::construct_payload(time_t iat, time_t exp)
{
    size_t len = (size_t)snprintf(nullptr, 0,
        "{\"iss\":\"%s\",\"sub\":\"%s\",\"scope\":\"%s\","
        "\"aud\":\"%s\",\"iat\":%ld,\"exp\":%ld}",
        JWT_ISSUER, JWT_SUBJECT, JWT_SCOPE, JWT_AUDIENCE,
        (long)iat, (long)exp);

    char *buf = (char *)k_malloc(len + 1);
    if (!buf) return nullptr;

    snprintf(buf, len + 1,
        "{\"iss\":\"%s\",\"sub\":\"%s\",\"scope\":\"%s\","
        "\"aud\":\"%s\",\"iat\":%ld,\"exp\":%ld}",
        JWT_ISSUER, JWT_SUBJECT, JWT_SCOPE, JWT_AUDIENCE,
        (long)iat, (long)exp);
    return buf;
}

char *JwtBuilder::build()
{
    time_t now = SystemClock::get();
    if (now <= 0) {
        LOG_ERR("Clock not set — cannot build JWT");
        return nullptr;
    }
    time_t exp = now + 3600;

    static constexpr int JWT_BUF = 2048;
    char *jwt = (char *)k_malloc(JWT_BUF);
    if (!jwt) return nullptr;

    size_t out_len = 0;
    int pos = 0;

    // 1. base64url-encode JWT header
    if (base64_encode((uint8_t *)(jwt + pos), (size_t)(JWT_BUF - pos), &out_len,
                      (const uint8_t *)JWT_HEADER, strlen(JWT_HEADER))) {
        LOG_ERR("Header encode failed");
        k_free(jwt);
        return nullptr;
    }
    pos += (int)out_len;
    jwt[pos++] = '.';

    // 2. base64url-encode JWT payload
    char *payload = construct_payload(now, exp);
    if (!payload) { k_free(jwt); return nullptr; }

    if (base64_encode((uint8_t *)(jwt + pos), (size_t)(JWT_BUF - pos), &out_len,
                      (const uint8_t *)payload, strlen(payload))) {
        LOG_ERR("Payload encode failed");
        k_free(payload);
        k_free(jwt);
        return nullptr;
    }
    k_free(payload);
    pos += (int)out_len;

    // 3. RS256 sign the header.payload portion
    unsigned char sig[MBEDTLS_PK_SIGNATURE_MAX_SIZE];
    unsigned char hash[32];
    size_t sig_len = 0;

    mbedtls_pk_context pk;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;

    mbedtls_pk_init(&pk);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                          nullptr, MBEDTLS_CTR_DRBG_MAX_SEED_INPUT);

    int rc = mbedtls_pk_parse_key(&pk,
                                  (const unsigned char *)private_key_pem,
                                  strlen((const char *)private_key_pem) + 1,
                                  nullptr, 0,
                                  mbedtls_ctr_drbg_random, &ctr_drbg);
    if (rc != 0) { 
        LOG_ERR("pk_parse_key: %d", rc); 
        goto fail; 
    }

    if (!mbedtls_pk_can_do(&pk, MBEDTLS_PK_RSA)) {
        LOG_ERR("Key is not RSA");
        goto fail;
    }

    mbedtls_sha256((const unsigned char *)jwt, (size_t)pos, hash, 0);

    rc = mbedtls_pk_sign(&pk, MBEDTLS_MD_SHA256, hash, 0,
                         sig, MBEDTLS_PK_SIGNATURE_MAX_SIZE, &sig_len,
                         mbedtls_ctr_drbg_random, &ctr_drbg);
    if (rc != 0) { LOG_ERR("pk_sign: %d (0x%x)", rc, -rc); goto fail; }

    // 4. Append "." + base64url-encode signature
    jwt[pos++] = '.';
    if (base64_encode((uint8_t *)(jwt + pos), (size_t)(JWT_BUF - pos), &out_len,
                      sig, sig_len)) {
        LOG_ERR("Signature encode failed");
        goto fail;
    }
    pos += (int)out_len;
    jwt[pos] = '\0';

    mbedtls_pk_free(&pk);
    mbedtls_entropy_free(&entropy);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_psa_crypto_free();
    return jwt;

fail:
    mbedtls_pk_free(&pk);
    mbedtls_entropy_free(&entropy);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_psa_crypto_free();
    k_free(jwt);
    return nullptr;
}
