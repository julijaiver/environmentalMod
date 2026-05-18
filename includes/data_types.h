#ifndef     DATA_TYPES_H_INCLUDED_
#define     DATA_TYPES_H_INCLUDED_

#include <time.h>

#define  TYPE_UNKNOWN    0
#define  TYPE_RUUVI_TAG  1
#define  TYPE_TEROS12    2
#define  TYPE_SOLYX14    3
#define  TYPE_SOLINST    4

// defines for log messagging
#define  TYPE_LOG_MSG_INT    254
#define  TYPE_LOG_MSG_TXT    255
#define  DR1_LEN             51
#define  MSG_TXT_MAX_LEN     (DR1_LEN - 1)
#define  MSG_INT_MAX_LEN     ((DR1_LEN - 2) / 4)


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
    //raw values from sensor
    float epsr;
    float temp;
    float bulk_ec;
    //calulated values
    float vwc;
    float pw_ec;
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

struct msg_log {
    int type;
    union {
        char txt_msg[MSG_TXT_MAX_LEN];
        struct {
            uint8_t id;
            uint8_t count;
            uint32_t int_vals[MSG_INT_MAX_LEN];
        } int_msg;
    };
};

#endif