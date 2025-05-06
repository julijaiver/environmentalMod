#ifndef DEVICE_H
#define DEVICE_H

#include <time.h>
#include "cJSON.h"

extern struct k_mutex json_mutex;

extern cJSON *root;

uint8_t setup(struct tm *time);
char* deleteChar(char* s, char ch);
int take_measurement(time_t timeout);
int send_data(const char *data);
void json_init(void);

void json_add_data(const char *mac, double temp, double humidity, double pressure);
void json_clean_data(void);
char *json_get_data_string(void);
int send_day_data(void);

#endif