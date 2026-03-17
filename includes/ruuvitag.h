#ifndef RUUVITAG_H
#define RUUVITAG_H

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <stdbool.h>

#define MANUFACTURER_ID 	0x9904
#define MIN_ADV_LENGTH 		7
#define RUUVI_RAWV2 		0x05
#define RUUVI_RAWV2_LENGTH 	24
#define FLAGS_LENGTH		2
#define FLAGS_TYPE			0x01
#define MD_TYPE				0xff
#define BT_ADDR_LE_STR_LEN	30
#define MAC_ADDRESS_LEN		18
#define RUUVITAG_COUNT		5

#define JSON_QUEUE_SIZE 10
typedef struct {
	char mac[20];
	double temp;
    double humidity;
    double pressure;
} json_data_t;

extern struct k_msgq json_msgq;

extern const char *ruuvitag_devices[RUUVITAG_COUNT];

extern int seen_count;



void parse_ruuvitag(uint8_t *data_ptr);
void scan_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type, struct net_buf_simple *ad);
int activate_bluetooth(void);
int start_ble_scan(void);
int stop_ble_scan(void);
void reset_seen_devices(void);
bool is_mac_seen(const char *addr);
void add_seen_mac(const char *addr);

#endif