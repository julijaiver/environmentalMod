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

// TODO: Try to shrink main function
int main(void)
{
	struct tm rtc_time;
	uint8_t err = setup(&rtc_time);

	if (err == ERR_NONE){
		int result = 0;
		k_mutex_init(&json_mutex);
		json_init();
		// Main loop
		while (1){
			k_msleep(1000);
			print_current_time();
			

			result = start_ble_scan();
			if (result == 0){
				time_t timeout = get_current_time() + 300; // 5 Minute timeout
				result = take_measurement(timeout);
				if (result != 0)
					printk("ERROR: Bluetooth Timeout\n");
				stop_ble_scan();
			}
			k_msleep(1000 * 300); // 5 Minute sleep
			json_clean_data();
		}
	}

	// Make error loop (blink led etc...)

	return 0;
}

