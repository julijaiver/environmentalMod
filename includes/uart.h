#ifndef UART_H
#define UART_H

#include <zephyr/kernel.h>

#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)
#define MSG_SIZE 1024

extern struct k_msgq uart_msgq;


void serial_cb(const struct device *dev, void *user_data);
void print_uart(const char *buf);

int uart_init(void);

#endif

