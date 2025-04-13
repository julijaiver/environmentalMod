#ifndef JWT_H
#define JWT_H

#include <stdint.h>
#include <time.h>

#define JWT_HEADER "{\"alg\": \"RS256\", \"typ\": \"JWT\"}"
#define JWT_ISSUER "thesis2025@prj-mtp-jaak-leht-ufl.iam.gserviceaccount.com"
#define JWT_SUBJECT "thesis2025@prj-mtp-jaak-leht-ufl.iam.gserviceaccount.com"
#define JWT_AUDIENCE "https://pubsub.googleapis.com/"

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