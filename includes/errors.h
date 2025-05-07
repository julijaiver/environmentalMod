#ifndef ERRORS_H
#define ERRORS_H

#include <stdlib.h>
#include <stdint.h>

// Errors for setup
#define ERR_NONE            0x00
#define ERR_UART_SETUP      0x01
#define ERR_BLUETOOTH_SETUP 0x02
#define ERR_MODEM_INIT      0x04
#define ERR_RTC_SET_TIME    0x08


void print_errors(uint8_t err);
void handle_errors(uint8_t *err, struct tm *time);

#endif