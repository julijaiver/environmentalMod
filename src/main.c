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
#include "cloud.h"

// TODO: Try to shrink main function
int main(void)
{
	struct tm rtc_time;
	uint8_t err = setup(&rtc_time);
	
	int tmp = 0;
	if(err == ERR_NONE){
		// Main loop
		time_t current_time = get_current_time();
		
		while(1){
			// TODO: Take first measurement
			k_msleep(1000);
			print_current_time();
			if(tmp == 0){
				const char *access_token = cloud_request_access_token();
				printk("Token: %s\n", access_token);
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

