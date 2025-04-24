#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/rtc.h>
#include <stdlib.h>
#include <time.h>
#include "uart.h"
#include "modem.h"
#include "cloud.h"
#include "jwt.h"
#include "errors.h"
#include "realtime.h"

uint8_t setup(struct tm *time);


// TODO: Try to shrink main function
int main(void)
{

	
	// TODO: Initialize UART
	// TODO: Initialize bluetooth
	// TODO: Initialize modem
	// TODO: Wait for 1 Minute for module to initialize
	// TODO: Set RTC up to date
	struct tm rtc_time;

	uint8_t err = setup(&rtc_time);
	
	int tmp = 0;
	if(err == ERR_NONE){
		// Main loop
		while(1){
			k_msleep(1000);
			print_current_time();
			if(tmp == 0){
				jwt_t access_token_jwt;
				jwt_init(&access_token_jwt);
				printk("iat: %lld, exp: %lld\n", access_token_jwt.payload.iat, access_token_jwt.payload.exp);
				char *res = jwt_build(JWT_HEADER, access_token_jwt.payload);
				printk("Signed: %s\n", res);
				tmp = 1;
			}
			// TODO: Take first measurement
			// TODO: Save measurement
			// TODO: Check if day has passed

			// TODO: Wait for 5 minutes -> loop back to measurement

		}
	}
	// Make error loop (blink led etc...)

	return 0;
}


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
			if(time_str != "ERROR"){
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