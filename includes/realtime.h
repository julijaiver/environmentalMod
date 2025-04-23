#ifndef REALTIME_H
#define REALTIME_H
#include <time.h>

void set_system_time(const char *str, struct tm *time);
void print_current_time();
void get_current_time(struct timespec *current_ts);
#endif