#ifndef REALTIME_H
#define REALTIME_H
#include <time.h>

void set_system_time(const char *str, struct tm *time);
void print_current_time(void);
int check_day_time(void);
time_t get_current_time(void);
void check_daily_data_upload(void);

#endif