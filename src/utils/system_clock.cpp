#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(system_clock);

#include <stdio.h>
#include <string.h>
#include <time.h>

#include <zephyr/kernel.h>

#include "utils/system_clock.hpp"

#include "utils/app_events.hpp"

time_t SystemClock::get()
{
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        return ts.tv_sec;
    }
    return (time_t)-1;
}

void SystemClock::print()
{
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        struct tm *t = gmtime(&ts.tv_sec);
        printk("Epoch: %lld\n", (long long)ts.tv_sec);
        printk("Now: %04d-%02d-%02d %02d:%02d:%02d\n",
               t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
               t->tm_hour, t->tm_min, t->tm_sec);
    } else {
        printk("ERROR: clock_gettime() failed\n");
    }
}

int SystemClock::set(const char *modem_time_str)
{
    if (!modem_time_str || modem_time_str[0] == '\0') return -EINVAL;

    int yy, mo, dd, hh, mm, ss, tz;
    int parsed = sscanf(modem_time_str, "%d/%d/%d,%d:%d:%d%d",
                        &yy, &mo, &dd, &hh, &mm, &ss, &tz);
    if (parsed < 6) {
        LOG_ERR("Cannot parse modem time: %s (got %d fields)", modem_time_str, parsed);
        return -EBADMSG;
    }

    struct tm t = {};
    t.tm_year = yy + 100;   // years since 1900; 
    t.tm_mon  = mo - 1;     // 0-based
    t.tm_mday = dd;
    t.tm_hour = hh;
    t.tm_min  = mm;
    t.tm_sec  = ss;

    // 
    time_t epoch = mktime(&t);
    if (epoch == (time_t)-1) {
        LOG_ERR("mktime failed for modem time: %s", modem_time_str);
        return -EINVAL;
    }

    struct timespec ts = { .tv_sec = epoch, .tv_nsec = 0 };
    if (clock_settime(CLOCK_REALTIME, &ts) != 0) {
        LOG_ERR("clock_settime failed");
        return -EIO;
    }

    LOG_INF("System clock set from modem: %s → epoch %lld",
            modem_time_str, (long long)epoch);
    AppEvents::clock_synced();
    return 0;
}
