#ifndef CJSON_HELPER_H
#define CJSON_HELPER_H

#include "cJSON.h"


extern struct k_mutex json_mutex;
extern cJSON *root;


void json_init(void);

void json_add_data(const char *mac, double temp, double humidity, double pressure);
void json_clean_data(void);
char *json_get_data_string(void);

#endif