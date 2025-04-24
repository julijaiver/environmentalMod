#ifndef JWT_H
#define JWT_H

#include <stdint.h>
#include <time.h>
#include "private.h"

#define JWT_HEADER          "{\"alg\":\"RS256\",\"kid\":\"" GOOGLE_PRIVATE_KEY_ID "\",\"typ\":\"JWT\"}"
#define JWT_ISSUER          "viherpysakki@prj-mtp-jaak-leht-ufl.iam.gserviceaccount.com"
#define JWT_SUBJECT         "viherpysakki@prj-mtp-jaak-leht-ufl.iam.gserviceaccount.com"
#define JWT_AUDIENCE        "https://oauth2.googleapis.com/token"
#define JWT_SCOPE           "https://www.googleapis.com/auth/pubsub"
#define JWT_SIGNATURE_SIZE  1024

// "https://pubsub.googleapis.com/"


//static const unsigned char* private_key = (unsigned char*) GOOGLE_PRIVATE_KEY;


//unsigned char           jwt[2048];

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
    unsigned char *signature;
} jwt_t;



void jwt_init(jwt_t *jwt);
char *jwt_build(unsigned char *header, struct payload payload);
char *jwt_construct_payload(struct payload *payload);

#endif