#ifndef JWT_H
#define JWT_H

#include <stdint.h>
#include <time.h>
#include "private.h"

#define JWT_SIGNATURE_SIZE  1024


struct payload{
    const char *audi;
    const char *iss;
    const char *sub;
    const char *scope;
    long long int iat;
    long long int exp;
};

typedef struct {
    unsigned char *header;
    struct payload payload;
} jwt_t;



void jwt_init(jwt_t *jwt);
char *jwt_build(unsigned char *header, struct payload payload);
char *jwt_construct_payload(struct payload *payload);

#endif