#pragma once

#include <time.h>

//builds a JWT from the secrets in private.h.
//only used with 4G modem path
class JwtBuilder {
    public:
        char *build();
    private:
        static char *construct_payload(time_t iat, time_t exp);
};
