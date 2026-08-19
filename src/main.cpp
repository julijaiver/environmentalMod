#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/rtc.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "sensor_scan/ruuvi_scan.hpp"
#include "sensor_scan/sdi12_scan.hpp"
#include "hardware/sdi12.hpp"
#include "pubsub/topic.hpp"
#include "transport/lora_statemachine.hpp"
#include "transport/lora_transport.hpp"
#include "serialization/binary_serializer.hpp"
#include "d_pipeline/cloud_sender.hpp"
#include "storage/nv_storage.hpp"
#include "utils/system_clock.hpp"

#if CONFIG_CLOUD_SEND_4G
#include "modem_transport.hpp"
#include "json_serializer.hpp"
#endif

#include "utils/app_events.hpp"

LOG_MODULE_REGISTER(main);

#define ADC_NODE DT_ALIAS(adc0)

#define CHANNEL_VREF(node_id) DT_PROP_OR(node_id, zephyr_vref_mv, 0)

static const struct device *adc = DEVICE_DT_GET(ADC_NODE);

static const struct adc_channel_cfg channel_cfgs[] = {
	DT_FOREACH_CHILD_SEP(ADC_NODE, ADC_CHANNEL_CFG_DT, (,))};

static uint32_t vrefs_mv[] = {DT_FOREACH_CHILD_SEP(ADC_NODE, CHANNEL_VREF, (,))};

#define CHANNEL_COUNT ARRAY_SIZE(channel_cfgs)

static constexpr float BAT_VOLTAGE_DIVIDER_RATIO = 5.6f;

// for shell access, global cause shell is not an object
extern void shell_nv_set(NvStorage &);
extern void shell_sdi12_set(SDI12Bus &);
extern void shell_lora_sm_set(LoRaStateMachine &);
extern void shell_cloud_sender_set(CloudSender &);

int main(void)
{
	NvStorage nv_storage;
	SDI12Bus sdi12_bus;
	LoRaStateMachine lora_sm;

	Topic<ruuvi_data> ruuvi_topic;
	Topic<teros12_data> teros_topic;
	Topic<solyx14_data> solyx_topic;
	Topic<solinst_data> solinst_topic;

	RuuviScanner ruuvi_scanner(ruuvi_topic, nv_storage);
	SDI12Scanner sdi12_scanner(sdi12_bus, ruuvi_scanner, teros_topic, solyx_topic, solinst_topic);

#if CONFIG_CLOUD_SEND_LORA
	LoRaTransport lora_transport(lora_sm);
	BinarySerializer binary_ser(ruuvi_topic, teros_topic, solyx_topic, solinst_topic);
	CloudSender cloud_sender(lora_transport, binary_ser);
#elif CONFIG_CLOUD_SEND_4G
	ModemTransport modem_transport;
	JsonSerializer json_ser(ruuvi_topic, teros_topic, solyx_topic, solinst_topic);
	CloudSender cloud_sender(modem_transport, json_ser);
#endif
	shell_nv_set(nv_storage);
	shell_sdi12_set(sdi12_bus);
	shell_lora_sm_set(lora_sm);
	shell_cloud_sender_set(cloud_sender);

	int rc;
#ifdef CONFIG_SEQUENCE_32BITS_REGISTERS
	uint32_t channel_reading[CONFIG_SEQUENCE_SAMPLES][CHANNEL_COUNT];
#else
	uint16_t channel_reading[CONFIG_SEQUENCE_SAMPLES][CHANNEL_COUNT];
#endif


	nv_storage.init();
	//check if lora appkey is set
	char lora_appkey[33];
	if (nv_storage.get_appkey(lora_appkey) <= 0)
	{
		LOG_WRN("LoRa AppKey not set in NVS. Use lora set_appkey <key> command to set it.");
	}

#if CONFIG_CLOUD_SEND_4G
	modem_transport.init();
	{
		int boot_rc = modem_transport.boot_setup();
		while (boot_rc != 0) {
			LOG_ERR("Modem boot_setup failed (%d), retrying in 30 s", boot_rc);
			k_msleep(30000);
			boot_rc = modem_transport.boot_setup();
		}
	}
	json_ser.init(ruuvi_topic, teros_topic, solyx_topic, solinst_topic);
#endif

#if 1
	printk("Type \"stop\" to stop boot\n");

	if(AppEvents::wait(AppEvents::BOOT_HALT | AppEvents::BOOT_CONTINUE, true, K_SECONDS(30)) == AppEvents::BOOT_HALT) {
		AppEvents::boot_wait();
	}

#endif
	AppEvents::boot_continue();

	const struct adc_sequence_options options = {
		.interval_us = 0,
		.extra_samplings = CONFIG_SEQUENCE_SAMPLES - 1,
	};

	struct adc_sequence sequence = {
		.options = &options,
		.buffer = channel_reading,
		.buffer_size = sizeof(channel_reading),
		.resolution = CONFIG_SEQUENCE_RESOLUTION,
	};

	if (!device_is_ready(adc)) {
		printf("ADC controller device %s not ready\n", adc->name);
		return 0;
	}

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


	AppEvents::clock_wait();

	SystemClock::print();
	while (1)
	{
		struct timespec now_ts;
		if (clock_gettime(CLOCK_REALTIME, &now_ts) == 0) {
			struct tm *t = gmtime(&now_ts.tv_sec);
			if(t->tm_min % AppEvents::MEASURE_CYCLE_MINUTES == 0) {
				LOG_INF("Start sampling");
				SystemClock::print();
				AppEvents::take_sample_now();
				rc = adc_read(adc, &sequence);
				if (rc >= 0) {
					for (size_t channel_index = 0U; channel_index < CHANNEL_COUNT; channel_index++) {
						int32_t val_mv;	

						// just read the first sample even if we have more than one

						val_mv = channel_reading[0][channel_index];

						rc = adc_raw_to_millivolts(vrefs_mv[channel_index],
										channel_cfgs[channel_index].gain,
										CONFIG_SEQUENCE_RESOLUTION, &val_mv);
						if ((rc < 0) || vrefs_mv[channel_index] == 0) {
							LOG_ERR("value in mV not available");
						} else {
							int bv = (int)(val_mv * BAT_VOLTAGE_DIVIDER_RATIO);
							cloud_sender.set_bat_voltage(bv);
							LOG_INF("Battery voltage: %d", bv);
						}

					}
				}
				else {
					LOG_ERR("Could not read ADC (%d)", rc);
				}

				k_msleep(AppEvents::MEASURE_CYCLE_MINUTES * 60000 - 5000); // go to sleep and wake up 5s before next deadline
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
