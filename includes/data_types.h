#ifndef     DATA_TYPES_H_INCLUDED_
#define     DATA_TYPES_H_INCLUDED_

#include <time.h>

#define  TYPE_RUUVI_TAG  0
#define  TYPE_TEROS12    1
#define  TYPE_SOLYX14    2

struct ruuvi_tag {
    float temperature;
    float pressure;
    float humidity;
    float bat_voltage;
};

struct teros12 {
    float vwc;
    float temp;
    float ec;
};

struct solyx14 {
    float perm;
    float temp;
    float ec;
};

struct sensor_data {
    int type;
    time_t timestamp;
    char id[32];
    union {
        struct ruuvi_tag ruuvi;
        struct teros12 teros;
        struct solyx14 solyx;
    };

};

#endif