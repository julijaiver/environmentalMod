#ifndef RSA_H
#define RSA_H
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "mbedtls/pk.h"
#include "mbedtls/md.h"

#define PRIVATE_KEY_FILE    "rsa.pem"
#define RSA_KEY_BITS        2048

int rsa_signature(const uint8_t *hash, size_t hash_len, uint8_t *sig, size_t *sig_len);

#endif