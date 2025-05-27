#ifndef DEVICE_H
#define DEVICE_H

#include <time.h>
#include "cJSON.h"

#define MODEM_PIN_NODE DT_PATH(zephyr_user)


int take_measurement();
int send_data(const char *data);
int send_day_data(void);
int startup_modem(void);
void modem_power_off(void);
void modem_pin_set(int state);

#endif