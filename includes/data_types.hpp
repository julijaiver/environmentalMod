#pragma once

#include <stdint.h>
#include <time.h>

#define TYPE_UNKNOWN 0
#define TYPE_RUUVI_TAG 1
#define TYPE_TEROS12 2
#define TYPE_SOLYX14 3
#define TYPE_SOLINST 4


#define DR1_LEN 51   // DR1 max payload

struct ruuvi_data {
    char mac[18];   // "AA:BB:CC:DD:EE:FF\0"
    time_t timestamp;
    float temperature;
    float pressure;
    float humidity;
    float bat_voltage;
};

struct teros12_data {
    char id[32];
    time_t timestamp;
    float vwc;
    float temp;
    float ec;
};

struct solyx14_data {
    char id[32];
    time_t timestamp;
    float epsr;
    float temp;
    float bulk_ec;
    float vwc;
    float pw_ec;
};

struct solinst_data {
    char id[32];
    time_t timestamp;
    float temp;
    float level;
    float compensated_level;
};


