#ifndef DEVICE_H
#define DEVICE_H

#include <stdlib.h>
#include <time.h>
#include "cJSON.h"

#define MEASURE_CYCLE_SLEEP 1000 * 300

#define MODEM_PIN_NODE DT_PATH(zephyr_user)

bool gpio_status(void);
uint16_t setup(struct tm *time);
int take_measurement();
int send_data(const char *data);
int send_day_data(void);
int startup_modem(void);
void modem_power_off(void);
void modem_pin_set(int state);

#endif