#ifndef SDI12_H_INCLUDED_
#define SDI12_H_INCLUDED_

#include <stdbool.h>

int sdi12_init(void);
int sdi12_cmd(const char *cmd, bool send_break);
int sdi12_wait_for(char *buffer, int size, const char *expect);
int sdi12_flush(void);

#endif