/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>

#define MANUFACTURER_ID 	0x9904
#define MIN_ADV_LENGTH 		7
#define RUUVI_RAWV2 		0x05
#define RUUVI_RAWV2_LENGTH 	24
#define FLAGS_LENGTH		2
#define FLAGS_TYPE			0x01
#define MD_TYPE				0xff

void print(){
	
}

static void parse_ruuvitag(uint8_t *data_ptr){
	uint8_t device_id[7]; // Device MAC-address
	float temperature;  // Temperature
	float humidity;     // Humidity
	float pressure;     // Pressure
	float accel_x;      // Accel x
	float accel_y;      // Accel y
	float accel_z;      // Accel z
	float voltage;      // Voltage

	int8_t power;      // TX power
	uint8_t counter;   // Movement counter
	uint16_t sequence; // Sequence number

    data_ptr++;
    temperature = (float)((int16_t)(*data_ptr << 8) + *(data_ptr + 1)) * (float)0.005;
    
    data_ptr += 2;
    humidity = (float)((uint16_t)(*data_ptr << 8) + *(data_ptr + 1)) * (float)0.0025;
    
    data_ptr += 2;
    pressure = (float)(((uint16_t)(*data_ptr << 8) + *(data_ptr + 1)) + 50000) / (float)100.0;
    
    data_ptr += 2;
    accel_x = (float)((int16_t)(*data_ptr << 8) + *(data_ptr + 1)) / (float)100.0;
    
    data_ptr += 2;
    accel_y = (float)((int16_t)(*data_ptr << 8) + *(data_ptr + 1)) / (float)100.0;
    
    data_ptr += 2;
    accel_z = (float)((int16_t)(*data_ptr << 8) + *(data_ptr + 1)) / (float)100.0;
    
    data_ptr += 2;
    voltage = (float)((uint16_t)(*data_ptr << 3) + (*(data_ptr + 1) >> 5)) / (float)1000.0 + (float)1.6;
    
    power = (*(data_ptr + 1) & 0x1f) * 2 - 40;
    
    data_ptr += 2;
    counter = *data_ptr;
    
    data_ptr++;
    sequence = ((uint16_t)(*data_ptr << 8) + *(data_ptr + 1));

    // MAC address is in following six bytes.
    data_ptr += 2;
	for(int i = 0; i < 6; i++){
		device_id[i] = *data_ptr;
		data_ptr++;
	}

	printk("DEVICE: %02X:%02X:%02X:%02X:%02X:%02X | Temperature: %.2f | Humidity: %.2f | Pressure: %.2f\n", device_id[0], device_id[1], device_id[2], device_id[3],
		device_id[4], device_id[5], temperature, humidity, pressure);


}

static void scan_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type, struct net_buf_simple *ad){
	if(ad->len < MIN_ADV_LENGTH){
		printk("AD: message too short\n"); 
		return;
	} 

	uint8_t *buffer_idx = ad->data;

	if(*buffer_idx != FLAGS_LENGTH){
		//printk("AD: no Flags structure\n"); 
		return;
	}

	buffer_idx++;
	if(*buffer_idx != FLAGS_TYPE){
		printk("AD: no Flags type\n"); 
		return;
	}
	buffer_idx += 2;

	uint8_t md_length = *buffer_idx;
	buffer_idx++;
	if(*buffer_idx != MD_TYPE){
		//printk("AD: no manufacturer data type\n"); 
		return;
	}

	buffer_idx++;
	uint16_t company_identifier = (*buffer_idx << 8) + *(buffer_idx + 1);
	if(company_identifier != MANUFACTURER_ID){
		printk("AD: no Ruuvi identifier\n"); 
		return;
	}

	if((ad->len - MIN_ADV_LENGTH) < (md_length - 3)){
		printk("Ruuvi: data too short - %d - %d\n", md_length -3, ad->len - MIN_ADV_LENGTH); 
		return;
	}
	if((ad->len - MIN_ADV_LENGTH) < RUUVI_RAWV2_LENGTH){
		printk( "Ruuvi: RAWv2 data too short\n"); 
		return;
	}

	buffer_idx += 2;
	parse_ruuvitag(buffer_idx);
}



int activateBluetooth(void)
{
	int err;

	/* Initialize the Bluetooth Subsystem */
	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return 0;
	}

	struct bt_le_scan_param scan_params = {
		.type 		= BT_LE_SCAN_TYPE_PASSIVE,
		.interval 	= BT_GAP_SCAN_FAST_INTERVAL,
		.window 	= BT_GAP_SCAN_FAST_WINDOW,
		.options 	= BT_LE_SCAN_OPT_FILTER_DUPLICATE,
	};

	err = bt_le_scan_start(&scan_params, scan_found);
	if(err){
		printk("Scanning failed to start ERROR: %d\n", err);
	} else {
		printk("Scanning started\n");
	}

	printk("Exiting %s thread.\n", __func__);
	return 0;
}
