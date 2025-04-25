// Standard libraries
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

// RTOS
#include <zephyr/kernel.h>

// Includes
#include "device.h"
#include "errors.h"
#include "uart.h"
#include "modem.h"
#include "realtime.h"


uint8_t setup(struct tm *time){
	uint8_t err = ERR_NONE;

	if (uart_init() != 0) {
		err |= ERR_UART_SETUP;
	} else {
		modem_status_t modem_ret = initialize_modem();
		if(modem_ret != MODEM_SUCCESS) {
			printk("Modem initialization failed: %s (%d)\n", modem_status_to_string(modem_ret), modem_ret);
			err |= ERR_MODEM_INIT;
			err |= ERR_RTC_SET_TIME;
		} else {
			k_msleep(1000 * 15);
			char time_str[32];
			size_t time_str_size = sizeof(time_str); 
			modem_get_time(time_str, time_str_size);
			if(strstr(time_str, "ERROR") == NULL){
				set_system_time(time_str, time);
			} else {
				err |= ERR_RTC_SET_TIME;
			}
		}
	}

	//if(activateBluetooth() != 0) err |= ERR_BLUETOOTH_SETUP;

	if(err != ERR_NONE){
		print_errors(err);
		handle_errors(err);
	}
	return err;
}