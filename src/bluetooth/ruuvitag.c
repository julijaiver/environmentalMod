/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <stdio.h>
#include "ruuvitag.h"
#include "device.h"

#if 0
static char seen_ruuvitag_devices[RUUVITAG_COUNT][MAC_ADDRESS_LEN];
static struct bt_le_scan_param scan_params = {
		.type 		= BT_LE_SCAN_TYPE_PASSIVE,
		.interval 	= BT_GAP_SCAN_FAST_INTERVAL,
		.window 	= BT_GAP_SCAN_FAST_WINDOW,
		.options 	= BT_LE_SCAN_OPT_FILTER_DUPLICATE,
	};


K_MSGQ_DEFINE(json_msgq, sizeof(json_data_t), JSON_QUEUE_SIZE, 4);
int seen_count = 0;
const char *ruuvitag_devices[RUUVITAG_COUNT] = {"DD:83:3D:A4:CE:C6", "DD:42:FA:12:2A:CD", "DB:C3:58:D9:03:70", "E2:70:D7:96:45:18", "EE:DF:9F:BA:8D:49"};

void parse_ruuvitag(uint8_t *data_ptr){
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
	char mac[20];
	snprintf(mac, 20, "%02X:%02X:%02X:%02X:%02X:%02X", device_id[0], device_id[1], device_id[2], device_id[3], device_id[4], device_id[5]);
	json_data_t data;
	strncpy(data.mac, mac, sizeof(data.mac));

	data.mac[sizeof(data.mac) - 1] = '\0';
	data.temp = temperature;
	data.humidity = humidity;
	data.pressure = pressure;

	if(k_msgq_put(&json_msgq, &data, K_NO_WAIT) != 0){
		printk("Failed to que JSON data\n");
	}
}

void scan_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type, struct net_buf_simple *ad){
	char addr_str[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
	
	for(int i = 0; i < RUUVITAG_COUNT; i++){
		if(strstr(addr_str, ruuvitag_devices[i]) != NULL){
			if(is_mac_seen(addr_str)) return;
			add_seen_mac(addr_str);
			
			if(ad->len < MIN_ADV_LENGTH){
				printk("AD: message too short\n"); 
				return;
			} 

			uint8_t *buffer_idx = ad->data;

			if(*buffer_idx != FLAGS_LENGTH){
				printk("AD: no Flags structure\n"); 
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
				printk("AD: no manufacturer data type\n"); 
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
	}
}


int activate_bluetooth(void)
{
	int err;

	/* Initialize the Bluetooth Subsystem */
	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return -1;
	}

	return 0;
}

int start_ble_scan(void){
	int err = bt_le_scan_start(&scan_params, scan_found);
	if(err) return -1;

	return 0;
}

int stop_ble_scan(void){
	printk("Stopping bluetooth scan\n");
	reset_seen_devices();
	int err = bt_le_scan_stop();
	if(err != 0) return -1;

	return 0;
}


void reset_seen_devices() {
    for (int i = 0; i < seen_count; i++) {
        seen_ruuvitag_devices[i][0] = '\0';
    }
	seen_count = 0;
}

bool is_mac_seen(const char *addr) {
	char *space = strchr(addr, ' ');
	if(space) *space = '\0';
	printk("Looking for %s in list:\n", addr);
    for (int i = 0; i < seen_count; i++) {
		printk("%s\n", seen_ruuvitag_devices[i]);
        if (strcmp(seen_ruuvitag_devices[i], addr) == 0) {
            return true;
        }
    }
	printk("\n");
    return false;
}

void add_seen_mac(const char *addr) {
    if (seen_count < RUUVITAG_COUNT) {
        strncpy(seen_ruuvitag_devices[seen_count], addr, MAC_ADDRESS_LEN);
		printk("Added: %s\n\n", addr);
        seen_ruuvitag_devices[seen_count][MAC_ADDRESS_LEN - 1] = '\0';
        seen_count++;
    }
}
#endif