#ifndef JWT_H
#define JWT_H

#include <stdint.h>
#include <time.h>
#include "private.h"

#define JWT_HEADER "{\"alg\": \"RS256\", \"typ\": \"JWT\", \"kid\": \"1c2c96db62b0d9927f7fb1445351f0f349a8b26b\"}"
#define JWT_ISSUER "thesis2025-457109@appspot.gserviceaccount.com"
#define JWT_SUBJECT "thesis2025-457109@appspot.gserviceaccount.com"
#define JWT_AUDIENCE "https://pubsub.googleapis.com/"
#define JWT_SIGNATURE_SIZE 1024


//static const unsigned char* private_key = (unsigned char*) GOOGLE_PRIVATE_KEY;
static const char *private_key_pem =
"-----BEGIN PRIVATE KEY-----\n"
"MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQCFCIgmb4anyZJd\n"
"BI5VRf8gdmuDgOqxwjxw4OxaxflAAFkotr9y/IJLnCaxWLSlGg/Z7+VDqb0hKLJY\n"
"9UqPwskrmDUKjon81JmQEeLBk6fcZ88njiindbHvqvcK8gIV6ybMAdi5woKoJELT\n"
"GnvpNVMozG8Lv/IvfD+6R/MZXdBEcUgbNOWeZM8S1LsYT+3ntTyvcBBREe/CZd9D\n"
"qjmtNksoGDxjOLdO5RlTGfwzAya8M2LO47sq6pD6wEUnRopDyzf0L/mXsiiOkbr/\n"
"w99h5Ctl0unC88FNn4ZGgMhLBbUo8hCXwgPGO2f7LLFF3/mpgi3bhyxr1FZD/qOD\n"
"3JKzvntxAgMBAAECggEAFwB0Jg0nf7BjHbExeP4K0E6ZZVnven/Gmo/RXhqX76B/\n"
"ygHbFWMiljcwG37mcRuR/RE1F19sY6TZPXdgBn5b8S02kpY1tqD90kK9bdH9dqb+\n"
"Uu0KC3ZWp0VsxJg2kxqfESwKkIfYtoDaiwyruxVsxQOf+aydD+fQTwGsv0iQv5yE\n"
"1JlGSxEl0Q7Mb6AFsXR94xwpLVMTJkxA2+Y3dFJznM4F4L6vyJDMWWH8MQP0vFUy\n"
"JQiKix9seV3IOasMS63pqd8KqaVRAnKoHY0wsOXlNXc1z5QeLdk1BG+odCnnh5Km\n"
"DIGjKXvtOEs6AbxuQHUUHbOA15Z6cmaLh5BL3sylFwKBgQC6dXRp/3Om6mAyoNXJ\n"
"C9CMwA1nDCBDYf8YYFSbO5fXj6ABTtl6WhevPg/pv1vA3pif6lbFcgplcBNHfEE3\n"
"MtP6frVrMkAPyzgABPeZ2/LH6ZSVvL01fVo/B0M9jdZ+AExTHf+/Ct32YyHt9ar6\n"
"vEj5wpkarUf5O/n8WoA/TJdXiwKBgQC2pire2/yUZwVV/4bEAg7hlBcp9t9AEAhz\n"
"xN4ZjrVIOYNL8x2EhYv6mTHAyDZGTz5IKx+dBdAmdpjZIHVkchKDFKgUAB8c+1c7\n"
"iLxiTaRZDQ8vGkPV7VCoBwce3eOvuI3Bj64Z9nO8wIzuPR5Hqe0RMqjQGf425iFp\n"
"/ddfv0t4cwKBgBqxUdcS0NWmW9sjlzdOz642mDSWUdATqVcuAy6t21DqqLdHOG8B\n"
"kr5tng2SbWow85yBCab+amqXHstvCE8EocAMf6A282DPcNbf3ypRiHICCFlfSZ7s\n"
"sQyw13lqYrhrBoInm0MYJSpuip7sOmvmpHPM3eopwFH3uhPcSNvG9St9AoGAJbdI\n"
"cP6iOI7RdkGutXjrU799zQeScCsfY7CKYGKson5l02Aff0cIcbYCpRlyw7AHX2Ww\n"
"QH97zQV+aI+gTh6UHgc88exYgGaSS9Pferknr8/Xi9VznpBDsH7LBJ+zLY0BkK+3\n"
"ttrCaX6lW2i5gPjg4EVCwL4tVW7OBZIKUFYzImsCgYEAt5HvrpRx0eUsR6ogNyUe\n"
"LtBwu3wOYhTP38UwU6puOfdnz2YZ4mAnK09wTZdjtEVJicMq+kuEzmMnFiBS9Icp\n"
"TWGh7VExJyYBuEsiiG45arklXBJAt1VCoFzhbV0euZiF+wh00xlQ5nXkYil278/U\n"
"i0mweirUSnnatTs6KqYeOVo=\n"
"-----END PRIVATE KEY-----\n";

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