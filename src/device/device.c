// Standard libraries
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

// RTOS
#include <zephyr/kernel.h>

// Includes
#include "device.h"
#include "errors.h"
#include "uart.h"
#include "modem.h"
#include "realtime.h"
#include "cloud.h"
#include "ruuvitag.h"

struct k_mutex json_mutex;
cJSON *root = NULL;

char* deleteChar(char* s, char ch) {
    int i, j;
    int len = strlen(s);
    for (i = j = 0; i < len; i++) {
        if (s[i] != ch) {
            s[j++] = s[i];
        }
    }
    s[j] = '\0';
    return s;
}

uint8_t setup(struct tm *time){
	uint8_t err = ERR_NONE;

	if (uart_init() != 0) {
		err |= ERR_UART_SETUP;
	} else {
		modem_status_t modem_ret = initialize_modem();
		if(modem_ret != MODEM_SUCCESS) {
			printk("Modem initialization failed: %s (%d)\n", modem_status_to_string(modem_ret), modem_ret);
			err |= ERR_MODEM_INIT;
			err |= ERR_RTC_SET_TIME;
		} else {
			k_msleep(1000 * 15); // Module needs some time to activate internet connection and clock sync
			char time_str[32];
			size_t time_str_size = sizeof(time_str); 
			modem_get_time(time_str, time_str_size);
			if(strstr(time_str, "ERROR") == NULL){
				set_system_time(time_str, time);
			} else {
				err |= ERR_RTC_SET_TIME;
			}
		}
	}

	if(activate_bluetooth() != 0) err |= ERR_BLUETOOTH_SETUP;

	if(err != ERR_NONE){
		print_errors(err);
		handle_errors(&err, time);
	}
	return err;
}

int take_measurement(){
	json_data_t data;
	int tries = 0;
	while(1){
		printk("In loop\n");
		if(k_msgq_get(&json_msgq, &data, K_NO_WAIT) == 0){
			json_add_data(data.mac, data.temp, data.humidity, data.pressure);
		}
		if(seen_count == RUUVITAG_COUNT){
			break;
		}
		if(tries >= 10){
			printk("Timeout\n");
			return -1;
		} 
		tries++;
		k_msleep(1000);
	}
	return 0;
}

int send_data(const char *data){
	const char *access_token = cloud_request_access_token();
	int result = cloud_publish(access_token, data);
	k_free(access_token);
	if(result != 0){
		return result;
	}
	
	return 0;
}

int send_day_data(void){
	int ret = 0;
	char *json_string = json_get_data_string();
	if (json_string != NULL){
		ret = send_data(json_string);
		if (ret != 0){
			printk("ERROR: Failed to publish data\n");
		}
	}
	else{
		ret = -1;
		printk("ERROR: Failed to get JSON string\n");
	}
	cJSON_free(json_string);
	return ret;
}

