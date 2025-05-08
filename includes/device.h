#ifndef DEVICE_H
#define DEVICE_H

#include <time.h>
#include "cJSON.h"

uint8_t setup(struct tm *time);
char* deleteChar(char* s, char ch);
int take_measurement();
int send_data(const char *data);
int send_day_data(void);

#endif