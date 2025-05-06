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
#define RUUVITAG_COUNT		2

#define JSON_QUEUE_SIZE 10
typedef struct {
	char mac[20];
	double temp;
    double humidity;
    double pressure;
} json_data_t;

extern struct k_msgq json_msgq;

static const char *ruuvitag_devices[RUUVITAG_COUNT] = {"DD:83:3D:A4:CE:C6", "DD:42:FA:12:2A:CD"};
static char seen_ruuvitag_devices[RUUVITAG_COUNT][MAC_ADDRESS_LEN];
extern int seen_count;

static struct bt_le_scan_param scan_params = {
		.type 		= BT_LE_SCAN_TYPE_PASSIVE,
		.interval 	= BT_GAP_SCAN_FAST_INTERVAL,
		.window 	= BT_GAP_SCAN_FAST_WINDOW,
		.options 	= BT_LE_SCAN_OPT_FILTER_DUPLICATE,
	};


void parse_ruuvitag(uint8_t *data_ptr);
void scan_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type, struct net_buf_simple *ad);
int activate_bluetooth(void);
int start_ble_scan(void);
int stop_ble_scan(void);
void reset_seen_devices(void);
bool is_mac_seen(const char *addr);
void add_seen_mac(const char *addr);

#endif