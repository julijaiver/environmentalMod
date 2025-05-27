#include <zephyr/kernel.h>

#include "cJSON.h"
#include "cJSON_helper.h"
#include "ruuvitag.h"

struct k_mutex json_mutex;
cJSON *root = NULL;

void json_init(void){
	k_mutex_lock(&json_mutex, K_FOREVER);
	
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
