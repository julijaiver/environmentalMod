#ifndef RUUVITAG_H
#define RUUVITAG_H

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <stdbool.h>

#define MANUFACTURER_ID 	0x0499
#define MIN_ADV_LENGTH 		7
#define RUUVI_RAWV2 		0x05
#define RUUVI_RAWV2_LENGTH 	24
#define FLAGS_LENGTH		2
#define FLAGS_TYPE			0x01
#define MD_TYPE				0xff
#define BT_ADDR_LE_STR_LEN	30
#define MAC_ADDRESS_LEN		18

#define MAX_RUUVI_TAG  10


int activate_bluetooth(void);
int start_ble_scan(void);
int stop_ble_scan(void);

#endif