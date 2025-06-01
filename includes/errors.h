#ifndef ERRORS_H
#define ERRORS_H

#include <stdlib.h>
#include <stdint.h>
#include <time.h>

// Errors for setup
#define ERR_NONE            0x00
#define ERR_UART_SETUP      0x01
#define ERR_BLUETOOTH_SETUP 0x02
#define ERR_MODEM_INIT      0x04
#define ERR_RTC_SET_TIME    0x08
#define ERR_MODEM_GPIO      0x10

#define RETRY_DELAY_MS      1000 * 10
#define MAX_TRIES           5
#define ERROR_LOOP_SLEEP    1000 * 30


void print_errors(uint16_t err);
void handle_errors(uint16_t *err, struct tm *time);

#endif