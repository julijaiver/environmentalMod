// RTOS
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/rtc.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>

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

LOG_MODULE_REGISTER(main);

/* ADC node from the devicetree. */
#define ADC_NODE DT_ALIAS(adc0)

/* Auxiliary macro to obtain channel vref, if available. */
#define CHANNEL_VREF(node_id) DT_PROP_OR(node_id, zephyr_vref_mv, 0)

/* Data of ADC device specified in devicetree. */
static const struct device *adc = DEVICE_DT_GET(ADC_NODE);

/* Data array of ADC channels for the specified ADC. */
static const struct adc_channel_cfg channel_cfgs[] = {
	DT_FOREACH_CHILD_SEP(ADC_NODE, ADC_CHANNEL_CFG_DT, (,))};

/* Data array of ADC channel voltage references. */
static uint32_t vrefs_mv[] = {DT_FOREACH_CHILD_SEP(ADC_NODE, CHANNEL_VREF, (,))};

/* Get the number of channels defined on the DTS. */
#define CHANNEL_COUNT ARRAY_SIZE(channel_cfgs)

#if CONFIG_SDI12
K_THREAD_STACK_DEFINE(sdi12_stack_area, SDI12_STACKSIZE);
struct k_thread sdi12_scan_thread_data;
k_tid_t sdi12_scan_thread_id;
#endif

K_THREAD_STACK_DEFINE(ruuvi_stack_area, RUUVI_STACKSIZE);
struct k_thread ruuvi_scan_thread_data;
k_tid_t ruuvi_scan_thread_id;


K_EVENT_DEFINE(envisens_events);

int bat_voltage;

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
	int rc;
#ifdef CONFIG_SEQUENCE_32BITS_REGISTERS
	uint32_t channel_reading[CONFIG_SEQUENCE_SAMPLES][CHANNEL_COUNT];
#else
	uint16_t channel_reading[CONFIG_SEQUENCE_SAMPLES][CHANNEL_COUNT];
#endif


	nv_params_init();
	//check if lora appkey is set 
	char lora_appkey[33];
	if (nvs_get_lora_appkey(lora_appkey) <= 0) 
	{
		LOG_WRN("LoRa AppKey not set in NVS. Use lora set_appkey <key> command to set it.");
	}

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

	if(k_event_wait(&envisens_events, BOOT_HALT_EVENT | BOOT_CONTINUE_EVENT, true, K_SECONDS(30)) == BOOT_HALT_EVENT) {
		BOOT_WAIT();
	}
	
#endif
	boot_continue();

	/* Options for the sequence sampling. */
	const struct adc_sequence_options options = {
		.extra_samplings = CONFIG_SEQUENCE_SAMPLES - 1,
		.interval_us = 0,
	};

	/* Configure the sampling sequence to be made. */
	struct adc_sequence sequence = {
		.buffer = channel_reading,
		/* buffer size in bytes, not number of samples */
		.buffer_size = sizeof(channel_reading),
		.resolution = CONFIG_SEQUENCE_RESOLUTION,
		.options = &options,
	};

	if (!device_is_ready(adc)) {
		printf("ADC controller device %s not ready\n", adc->name);
		return 0;
	}

	/* Configure channels individually prior to sampling. */
	for (size_t i = 0U; i < CHANNEL_COUNT; i++) {
		sequence.channels |= BIT(channel_cfgs[i].channel_id);
		rc = adc_channel_setup(adc, &channel_cfgs[i]);
		if (rc < 0) {
			printf("Could not setup channel #%d (%d)\n", i, rc);
			return 0;
		}
		if ((vrefs_mv[i] == 0) && (channel_cfgs[i].reference == ADC_REF_INTERNAL)) {
			vrefs_mv[i] = adc_ref_internal(adc);
		}
	}


	uint16_t err = setup(&rtc_time); // this mostly 4G-modem related setup

	while (err != ERR_NONE)
	{
		printk("Error loop\n");
		k_msleep(ERROR_LOOP_SLEEP);
		handle_errors(&err, &rtc_time);
	}

	CLOCK_WAIT();

	print_current_time();
	// Main loop
	while (1)
	{
		struct timespec now_ts;
		if (clock_gettime(CLOCK_REALTIME, &now_ts) == 0) {
			struct tm *t = gmtime(&now_ts.tv_sec);
			if(t->tm_min % MEASURE_CYCLE_MINUTES == 0) {
				LOG_INF("Start sampling");
				print_current_time();
				TAKE_SAMPLE_NOW(); 
				rc = adc_read(adc, &sequence);
				if (rc >= 0) {
					for (size_t channel_index = 0U; channel_index < CHANNEL_COUNT; channel_index++) {
						int32_t val_mv;	

						// just read the first sample even if we have more than one
						// todo: cleanup setup
						val_mv = channel_reading[0][channel_index];

						rc = adc_raw_to_millivolts(vrefs_mv[channel_index],
										channel_cfgs[channel_index].gain,
										CONFIG_SEQUENCE_RESOLUTION, &val_mv);
						/* conversion to mV may not be supported, skip if not */
						if ((rc < 0) || vrefs_mv[channel_index] == 0) {
							LOG_ERR("value in mV not available");
						} else {
							// todo: report battery voltage
							bat_voltage = (int)(val_mv * 5.6);
							LOG_INF("Battery voltage: %d", bat_voltage); // multiplier 5.6 based on voltage divider

						}
					
					}
				}
				else {
					LOG_ERR("Could not read ADC (%d)", rc);
				}

				k_msleep(MEASURE_CYCLE_MINUTES * 60000 - 5000); // go to sleep and wake up 5s before next deadline
			}
			else {
				k_msleep(100); // not there yet - sleep just a bit and check again
			}
		}
		else {
			k_msleep(60000);
		}
	}

	return 0;
}
