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


int main(void){
	struct tm rtc_time;
	uint16_t err = setup(&rtc_time);
	
	while(err != ERR_NONE){
		printk("Error loop\n");
		k_msleep(ERROR_LOOP_SLEEP);
		handle_errors(&err, &rtc_time);
	}

	int result = 0;
	// Main loop
	while (1){
		print_current_time();

		result = start_ble_scan();
		if (result == 0){
			result = take_measurement();
			
			if (result == -1){
				printk("ERROR: Bluetooth Timeout\n");
			}
		}
		stop_ble_scan();
		
		//check_daily_data_upload();  //Uncomment to use the older daily method
		check_scheduled_upload();  
		printk("Going to sleep\n");
		k_msleep(MEASURE_CYCLE_SLEEP); // 5 Minute sleep
	}

	return 0;
}

