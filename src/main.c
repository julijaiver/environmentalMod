// RTOS
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/rtc.h>

// Standard libraries
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Includes
#include "common.h"
#include "errors.h"
#include "realtime.h"
#include "device.h"
#include "ruuvitag.h"
#include "uart.h"
#include "sdi12_scan.h"
#include "ruuvi_scan.h"
#include "nv_params.h"

#if CONFIG_SDI12
K_THREAD_STACK_DEFINE(sdi12_stack_area, SDI12_STACKSIZE);
struct k_thread sdi12_scan_thread_data;
k_tid_t sdi12_scan_thread_id;
#endif

K_THREAD_STACK_DEFINE(ruuvi_stack_area, RUUVI_STACKSIZE);
struct k_thread ruuvi_scan_thread_data;
k_tid_t ruuvi_scan_thread_id;


K_EVENT_DEFINE(envisens_events);

void boot_halt(void)
{
    k_event_post(&envisens_events, BOOT_HALT_EVENT);
}

void boot_continue(void)
{
    k_event_post(&envisens_events, BOOT_CONTINUE_EVENT);
}

int main(void)
{
	struct tm rtc_time;

	nv_params_init();

#if CONFIG_SDI12
	sdi12_scan_thread_id = k_thread_create(&sdi12_scan_thread_data, sdi12_stack_area,
										   K_THREAD_STACK_SIZEOF(sdi12_stack_area),
										   sdi12_scan_thread,
										   NULL, NULL, NULL,
										   SDI12_THREAD_PRIORITY, 0, K_NO_WAIT);
	k_thread_name_set(sdi12_scan_thread_id, "sdi12_scan");
#endif
#if CONFIG_RUUVI_TAG
	ruuvi_scan_thread_id = k_thread_create(&ruuvi_scan_thread_data, ruuvi_stack_area,
										   K_THREAD_STACK_SIZEOF(ruuvi_stack_area),
										   ruuvi_scan_thread,
										   NULL, NULL, NULL,
										   RUUVI_THREAD_PRIORITY, 0, K_NO_WAIT);
	k_thread_name_set(ruuvi_scan_thread_id, "ruuvi_scan");
#endif

#if 1
	printk("Type \"stop\" to stop boot\n");

	if(k_event_wait(&envisens_events, BOOT_HALT_EVENT, true, K_SECONDS(30)) == BOOT_HALT_EVENT) {
		BOOT_WAIT();
	}
	
#endif
	boot_continue();

	uint16_t err = setup(&rtc_time); // this mostly 4G-modem related setup

	while (err != ERR_NONE)
	{
		printk("Error loop\n");
		k_msleep(ERROR_LOOP_SLEEP);
		handle_errors(&err, &rtc_time);
	}
	// Main loop
	while (1)
	{
		print_current_time();

		printk("Going to sleep\n");
		k_msleep(MEASURE_CYCLE_SLEEP); // 5 Minute sleep
	}

	return 0;
}
