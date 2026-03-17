#ifndef DEVICE_H
#define DEVICE_H

#include <stdlib.h>
#include <time.h>

#define MEASURE_CYCLE_SLEEP (1000 * 60 * 5)
#define MAX_MEASUREMENTS    300
#define MODEM_PIN_NODE DT_PATH(zephyr_user)

typedef struct {
    char mac[18];
    float temperature[MAX_MEASUREMENTS];
    float humidity[MAX_MEASUREMENTS];
    float pressure[MAX_MEASUREMENTS];
    int measure_count;
} SensorData;

bool gpio_status(void);
uint16_t setup(struct tm *time);
int take_measurement(void);
int send_data(void);
int send_scheduled_data(void); 
int startup_modem(void);
void modem_power_off(void);
void modem_pin_set(int state);
char *deleteChar(char *s, char ch);

int find_sensor(SensorData *sensors, const char *mac);
void init_sensors(void);
void json_data_add(SensorData *sensors, const char *mac, float temperature, float humidity, float pressure);
char* json_data_string(int count);
void clean_data(void);

#endif