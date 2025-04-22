#ifndef JWT_H
#define JWT_H

#include <stdint.h>
#include <time.h>
#include "private.h"

#define JWT_HEADER "{\"alg\": \"RS256\", \"typ\": \"JWT\", \"kid\": \"" GOOGLE_PRIVATE_KEY_ID "\"}"
#define JWT_ISSUER "thesis2025-457109@appspot.gserviceaccount.com"
#define JWT_SUBJECT "thesis2025-457109@appspot.gserviceaccount.com"
#define JWT_AUDIENCE "https://pubsub.googleapis.com/"
#define JWT_SIGNATURE_SIZE 1024


//static const unsigned char* private_key = (unsigned char*) GOOGLE_PRIVATE_KEY;


//unsigned char           jwt[2048];

struct payload{
    const char *audi;
    const char *iss;
    const char *sub;
    uint32_t iat;
    uint32_t exp;
};

typedef struct {
    const char *header;
    struct payload payload;
    char *signature;
} jwt_t;



void jwt_init(jwt_t *jwt);
char *jwt_build(char *header, struct payload payload);
char *jwt_construct_payload(struct payload *payload);

#endif