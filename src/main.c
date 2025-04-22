#include <zephyr/kernel.h>
#include <stdlib.h>
#include "uart.h"
#include "modem.h"
#include "cloud.h"
#include "jwt.h"
#include "errors.h"

uint8_t setup();

// TODO: Try to shrink main function
int main(void)
{

	
	// TODO: Initialize UART
	// TODO: Initialize bluetooth
	// TODO: Initialize modem
	// TODO: Wait for 1 Minute for module to initialize
	// TODO: Set RTC up to date
	uint8_t err = setup();
	
	

	
	// TODO: Check for errors
	// TODO: If errors handle them and try to initialize again for 10 times over 5min duration
	

	// TODO: Take first measurement
	// TODO: Save measurement
	// TODO: Check if day has passed

	// TODO: Wait for 5 minutes -> loop back to measurement



	k_msleep(2000);
	printk("Initialize JWT\n");
	jwt_t access_token_jwt;
	jwt_init(&access_token_jwt);
	printk("Ready\n");
	printk("Build JWT\n");
	char *res = jwt_build(JWT_HEADER, access_token_jwt.payload);
	printk("Result: %s\n", res);

	/*
	int uart_ret = uart_init();
	if (uart_ret < 0) {
		printk("UART initialization failed: %d\n", uart_ret);
	} else {
		printk("UART initialization successful\n");

		printk("Initializing modem...\n");

		// TODO: add timer to send only once a day
		modem_status_t modem_ret = initialize_modem();
		if(modem_ret != MODEM_SUCCESS) {
			printk("Modem initialization failed: %s (%d)\n", modem_status_to_string(modem_ret), modem_ret);
		} else {
			printk("Modem initialization successful\n");

			// TODO: move these to a separate function
			
			const char *data = "{\"data\":\"value\"}";

			printk("Sending HTTP POST request to %s\n", CLOUD_HOST);
			modem_ret = send_http_post(CLOUD_HOST, data);
			if(modem_ret != MODEM_SUCCESS) {
				printk("HTTP POST failed: %s (%d)\n", modem_status_to_string(modem_ret), modem_ret);
			} else {
				printk("HTTP POST successful\n");
			
		}
	}
	*/

	// TODO: take measurements every 5 minutes

	return 0;
}


uint8_t setup(){
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
			// sleep for 1min for modem to initialize properly
			k_msleep(1000 * 60);

		}
	}

	if(activateBluetooth() != 0) err |= ERR_BLUETOOTH_SETUP;

	if(err != ERR_NONE){
		print_erros(err);
		handle_errors(err);
	}
	return err;
}