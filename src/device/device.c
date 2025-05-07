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

int take_measurement(time_t timeout){
	time_t current_time = get_current_time();
	json_data_t data;

	while(1){
		if(k_msgq_get(&json_msgq, &data, K_FOREVER) == 0){
			json_add_data(data.mac, data.temp, data.humidity, data.pressure);
		}
		if(seen_count == RUUVITAG_COUNT){
			break;
		}
		if(current_time >= timeout) return -1;
		k_msleep(100);
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

void json_init(void){
	k_mutex_lock(&json_mutex, K_FOREVER);
	/* cJSON_Hooks hooks = {
		.malloc_fn = k_malloc,
		.free_fn = k_free
	};
	cJSON_InitHooks(&hooks); */
	
	root = cJSON_CreateObject();
	if(root == NULL){
		printk("ERROR: cJSON_CreateObject failed! Root is NULL\n");
		return;
	}
	for(int i = 0; i < RUUVITAG_COUNT; i++){
		cJSON *device = cJSON_CreateObject();

		cJSON_AddArrayToObject(device, "temperature");
		cJSON_AddArrayToObject(device, "humidity");
		cJSON_AddArrayToObject(device, "pressure");

		cJSON_AddItemToObject(root, ruuvitag_devices[i], device);
	}
	char *ini = cJSON_PrintUnformatted(root);
	//printk("JSON INITIALZIED: %s\n", ini);
	cJSON_free(ini);
	k_mutex_unlock(&json_mutex);
}

void json_add_data(const char *mac, double temp, double humidity, double pressure){
	k_mutex_lock(&json_mutex, K_FOREVER);
	if(!root){
		printk("ERROR: Root NULL\n");
		return;
	}

	cJSON *device = cJSON_GetObjectItem(root, mac);	
	//printk("Got device: %s\n", device->string);
	if(!device) {
		printk("ERROR: Device NULL\n");
		return;
	}


	cJSON *temperature_array = cJSON_GetObjectItem(device, "temperature");
	cJSON *humidity_array = cJSON_GetObjectItem(device, "humidity");
	cJSON *pressure_array = cJSON_GetObjectItem(device, "pressure");

	if(!temperature_array || !humidity_array || !pressure_array) {
		printk("ERROR: Array NULL\n");
		return;
	}


	cJSON_AddItemToArray(temperature_array, cJSON_CreateNumber(temp));
	cJSON_AddItemToArray(humidity_array, cJSON_CreateNumber(humidity));
	cJSON_AddItemToArray(pressure_array, cJSON_CreateNumber(pressure));
	k_mutex_unlock(&json_mutex);
}

void json_clean_data(void){
	k_mutex_lock(&json_mutex, K_FOREVER);
	if (!root) return;
	

    cJSON *device = NULL;
    cJSON_ArrayForEach(device, root) {
        if (!cJSON_IsObject(device)) continue;

        cJSON_ReplaceItemInObject(device, "temperature", cJSON_CreateArray());
        cJSON_ReplaceItemInObject(device, "pressure", cJSON_CreateArray());
        cJSON_ReplaceItemInObject(device, "humidity", cJSON_CreateArray());
    }
	k_mutex_unlock(&json_mutex);
}

char *json_get_data_string(void){
	k_mutex_lock(&json_mutex, K_FOREVER);
	if(!root)
	{
		printk("ERROR: ROOT NULL\n");
		k_mutex_unlock(&json_mutex);
		return NULL;
	}	
	char *json_str = cJSON_PrintUnformatted(root);
	k_mutex_unlock(&json_mutex);
	return json_str;
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

