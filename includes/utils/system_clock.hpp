#pragma once

#include <time.h>

class SystemClock {
    public:
    //parse time string, call clock_settime(), then set CLOCK_SYNCED app evt
        static time_t get();
        static void print();
        static int set(const char *modem_time_str);
};