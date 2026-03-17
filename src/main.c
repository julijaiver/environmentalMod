// RTOS
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/rtc.h>

// Standard libraries
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Includes
#include "errors.h"
#include "realtime.h"
#include "device.h"
#include "ruuvitag.h"
#include "uart.h"
#include "sdi12_scan.h"
#include "nv_params.h"

K_THREAD_STACK_DEFINE(sdi12_stack_area, SDI12_STACKSIZE);
struct k_thread sdi12_scan_thread_data;
k_tid_t sdi12_scan_thread_id;

int main(void)
{
	struct tm rtc_time;

	nv_params_init();

	uint16_t err = setup(&rtc_time);

	while (err != ERR_NONE)
	{
		printk("Error loop\n");
		k_msleep(ERROR_LOOP_SLEEP);
		handle_errors(&err, &rtc_time);
	}

	sdi12_scan_thread_id = k_thread_create(&sdi12_scan_thread_data, sdi12_stack_area,
										   K_THREAD_STACK_SIZEOF(sdi12_stack_area),
										   sdi12_scan_thread,
										   NULL, NULL, NULL,
										   SDI12_THREAD_PRIORITY, 0, K_NO_WAIT);

	// Main loop
	while (1)
	{
		print_current_time();
#if 0  // temporary disable ble
		int result = start_ble_scan();
		if (result == 0)
		{
			result = take_measurement();

			if (result == -1)
			{
				printk("ERROR: Bluetooth Timeout\n");
			}
		}
		stop_ble_scan();

		// check_daily_data_upload();  //Uncomment to use the older daily method
		check_scheduled_upload();
#endif
		printk("Going to sleep\n");
		k_msleep(MEASURE_CYCLE_SLEEP); // 5 Minute sleep
	}

	return 0;
}
