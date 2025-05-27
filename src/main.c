// RTOS
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/rtc.h>

// Standard libraries
#include <stdlib.h>
#include <time.h>

// Includes
#include "errors.h"
#include "realtime.h"
#include "device.h"
#include "ruuvitag.h"
#include "uart.h"
#include "cJSON_helper.h"


int main(void)
{
	printk("Starting up\n");
	struct tm rtc_time;
	uint8_t err = setup(&rtc_time);
	

	if (err == ERR_NONE){
		int result = 0;

		k_mutex_init(&json_mutex); // needed because cJSON is not multithread safe
		json_init();
		// Main loop
		while (1){
			print_current_time();

			result = start_ble_scan();
			if (result == 0){
				printk("Taking measurement\n");
				result = take_measurement();
				if (result == -1){
					printk("ERROR: Bluetooth Timeout\n");
				}
				stop_ble_scan();
			}
			check_daily_data_upload();
			printk("Going to sleep\n");
			k_msleep(1000 * 300); // 5 Minute sleep
		}
	}

	if(err != ERR_NONE){

	}
	// Make error loop (blink led etc...)

	return 0;
}

