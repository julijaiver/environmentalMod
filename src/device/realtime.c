// Standard libraries
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

// nrf SDK
#include <sys/time.h>

// Includes
#include "realtime.h"
#include "modem.h"
#include "device.h"
#include "private.h"
#include "cJSON_helper.h"


static time_t last_sent_day = 0;

void set_system_time(const char *str, struct tm *time){
     if(!str || !time) return;

    int year, month, day, hour, min, sec;

    int parsed = sscanf(str, "%2d/%2d/%2d,%2d:%2d:%2d", &year, &month, &day, &hour, &min, &sec);

    // Converting two digit year to rigth format
    year += 2000;

    time->tm_year = year - 1900; // RTC uses years sence 1900
    time->tm_mon = month - 1; // starts from 0
    time->tm_mday = day;
    time->tm_hour = hour;
    time->tm_min = min;
    time->tm_sec = sec;
    time->tm_isdst = 0;

   

    time_t epoch = mktime(time);
    if(epoch == (time_t)-1){
        printk("Invalid time struct\n");
        return;
    }

    struct timespec ts = {
        .tv_sec = epoch,
        .tv_nsec = 0
    };

    if(clock_settime(CLOCK_REALTIME, &ts) != 0){
        printk("Failed to set system time\n");
    } else {
        printk("system time set successfully\n");
    }
}


void print_current_time(void){
    struct timespec now_ts;
    if (clock_gettime(CLOCK_REALTIME, &now_ts) == 0) {
        struct tm *t = gmtime(&now_ts.tv_sec);
        printk("Epoch: %lld\n", now_ts.tv_sec);
        printk("Now: %04d-%02d-%02d %02d:%02d:%02d\n",
               t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
               t->tm_hour, t->tm_min, t->tm_sec);
    } else {
        printk("ERROR: clock_gettime() failed\n");
    }
}

void check_daily_data_upload(void){
    struct timespec now_ts;
    if(clock_gettime(CLOCK_REALTIME, &now_ts) == 0){
        struct tm *t = gmtime(&now_ts.tv_sec);
        printk("HOUR: %d, SENDING HOUR: %d\n", t->tm_hour, TIME_TO_SEND);
        if(t->tm_hour == TIME_TO_SEND && t->tm_mday != last_sent_day){
            startup_modem();
            send_day_data();
            last_sent_day = t->tm_mday;
            json_clean_data();
            modem_power_off();
        } 
    } else {
        printk("Not time to send\n");
    } 
}


time_t get_current_time(void){
    struct timespec tmp_ts;
    if(clock_gettime(CLOCK_REALTIME, &tmp_ts) == 0){
        return tmp_ts.tv_sec;
    }
    return (time_t)-1;
}