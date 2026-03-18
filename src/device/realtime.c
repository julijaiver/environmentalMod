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

static time_t last_upload_time = 0;
static int upload_interval_seconds = SENDING_INTERVAL * 3600; // should replace with macro later
static time_t last_sent_day = 0;

#define LOCAL_UTC_OFFSET   2 
#define DST_CALENDAR_SIZE  10
#define DST_CAL_Y  0
#define DST_CAL_M  1
#define DST_CAL_D  2


static const int dst_start_EU[DST_CALENDAR_SIZE][3] = {
    { 2026, 3, 29},
    { 2027, 3, 28},
    { 2028, 3, 26},
    { 2029, 3, 25},
    { 2030, 3, 31},
    { 2031, 3, 30},
    { 2032, 3, 28},
    { 2033, 3, 27},
    { 2034, 3, 26},
    { 2035, 3, 25}
};

static const int dst_end_EU[DST_CALENDAR_SIZE][3] = {
    { 2026, 10, 25},
    { 2027, 10, 31},
    { 2028, 10, 29},
    { 2029, 10, 28},
    { 2030, 10, 27},
    { 2031, 10, 26},
    { 2032, 10, 31},
    { 2033, 10, 30},
    { 2034, 10, 29},
    { 2035, 10, 28}
};

static int get_utc_offset(int y, int m, int d, int h, int min)
{
    // the time above is local time that we get from the network
    // in order to convert it to UTC we first need to figure out if DST is used or not
    // there is some uncertainty at the end of DST since the modem does not report
    // whether DST is active or not. When turning back the clock we go through same
    // hour twice: once with DST applied and second time without
    // if we have such bad luck maybe we should reboot after one hour to be sure
    // that we got the right time since sync the time after each reboot
    // the other option is to use an ntp-server to sync the clock

    int i = 0;

    // dst starts and ends always in the same month so we just use the first entry see if we are not in a transition month
    // if month is not in DST period return local offset
    if(m < dst_start_EU[i][DST_CAL_M] || m > dst_end_EU[i][DST_CAL_M]) return LOCAL_UTC_OFFSET;
    // if month is in DST period but not in transition months return DST offset
    if(m < dst_start_EU[i][DST_CAL_M] || m > dst_end_EU[i][DST_CAL_M]) return LOCAL_UTC_OFFSET+1;

    // find if the year is in our DST calendar
    while(i < DST_CALENDAR_SIZE && y != dst_start_EU[i][DST_CAL_Y]) {
        ++i;
    }
    // if we are outside of our DST calendar, just return use first entry and hope for the best
    // it is unlikely that this SW will be in active use after the end of the calendar
    if(i >= DST_CALENDAR_SIZE) i = 0;
    // start month
    if(m == dst_start_EU[i][DST_CAL_M]) {
        if(d < dst_start_EU[i][DST_CAL_D]) return LOCAL_UTC_OFFSET;
        if(d > dst_start_EU[i][DST_CAL_D]) return LOCAL_UTC_OFFSET+1;
        // transition date
        if(h>1) return LOCAL_UTC_OFFSET+1;
        return LOCAL_UTC_OFFSET;
    }
    // must be the end month if we got this far
    if(d < dst_end_EU[i][DST_CAL_D]) return LOCAL_UTC_OFFSET+1;
    if(d > dst_end_EU[i][DST_CAL_D]) return LOCAL_UTC_OFFSET;
    // transition date: transition backward happens at 01.00 UTC 
    if(h > (1 + LOCAL_UTC_OFFSET+1)) return LOCAL_UTC_OFFSET;
    // definitely before transition
    if(h < (LOCAL_UTC_OFFSET+1)) return LOCAL_UTC_OFFSET + 1;
    // we have one hour uncertainty here since we don't know if DST was in effect or not
    // we see the same time twice
    return LOCAL_UTC_OFFSET+1;
}

void set_system_time(const char *str, struct tm *time){
     if(!str || !time) return;
 
    int year, month, day, hour, min, sec;

    int parsed = sscanf(str, "%2d/%2d/%2d,%2d:%2d:%2d", &year, &month, &day, &hour, &min, &sec);

    if(parsed != 6) {
        printk("Invalid time format\n");
        return;
    }

    int offset = get_utc_offset(year, month, day, hour, min);

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

    epoch -= offset * 3600; // set epoch to UTC time

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
            int tries = 0;
            int res = -1;
            while(res != 0 && tries <= 10){
                startup_modem();
                res = send_scheduled_data(); 
                tries++;
                k_msleep(5000);
            }
            if(res == 0) {
                last_sent_day = t->tm_mday;
                clean_data();
            } else {
                printk("Failed to send\n");
            }
            modem_power_off();
        } 
    } else {
        printk("Not time to send\n");
    } 
}


//Mimic the original function to avoid new edge cases
void check_scheduled_upload(void) {
    struct timespec now_ts;
    if(clock_gettime(CLOCK_REALTIME, &now_ts) == 0) {
        time_t current_time = now_ts.tv_sec;
        // Check if enough time has passed since last upload or if it's the first upload
        if((current_time - last_upload_time >= upload_interval_seconds) || (last_upload_time == 0) ) {
            printk("Time for upload. Last: %ld, Current: %ld, Interval: %d\n", 
                   (long int)last_upload_time, (long int)current_time, upload_interval_seconds);
            
            int tries = 0;
            int res = -1;
            while(res != 0 && tries <= 10) {
                startup_modem();
                res = send_scheduled_data();
                tries++;
                if(res!=0){k_msleep(5000);} //Since modem is the energy hog, might as well end loop sooner if done
            }
            
            if(res == 0) {
                last_upload_time = current_time;
                //clean_data(); //duplicate function call, already done in send_scheduled_data if successful
            } else {
                printk("Failed to send.\n");
            }
            modem_power_off();
        }
    } else {
        printk("Clock error\n");
    }
}


time_t get_current_time(void){
    struct timespec tmp_ts;
    if(clock_gettime(CLOCK_REALTIME, &tmp_ts) == 0){
        return tmp_ts.tv_sec;
    }
    return (time_t)-1;
}