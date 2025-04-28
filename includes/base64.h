#ifndef BASE64_H
#define BASE64_H

#include <stddef.h>


char *base64_encrypt(unsigned char *str, size_t len, int mode);

#endif