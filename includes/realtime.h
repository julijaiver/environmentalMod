#ifndef REALTIME_H
#define REALTIME_H
#include <time.h>

void set_system_time(const char *str, struct tm *time);
void print_current_time();
time_t get_current_time();

#endif