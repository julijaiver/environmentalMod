// Standard libraries
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

// RTOS
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

// Includes
#include "device.h"
#include "errors.h"
#include "uart.h"
#include "modem.h"
#include "realtime.h"
#include "cloud.h"
#include "ruuvitag.h"

static const struct gpio_dt_spec modem_gpio = GPIO_DT_SPEC_GET(MODEM_PIN_NODE, modem_pin_gpios);
//static SensorData sensors[RUUVITAG_COUNT];
//static char json_buf[4096]; // original 40960
//static uint16_t batch_number = 0; // keeps track of measurement batches within an upload. This lets you separate which measurements came in which measurement cycle.

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

uint16_t setup(struct tm *time){
	uint16_t err = ERR_NONE;

	// Initialize GPIO for modem control
	if (!device_is_ready(modem_gpio.port)) {
		printk("Modem GPIO device not ready\n");
		err |= ERR_MODEM_GPIO;
	}

	int ret = gpio_pin_configure_dt(&modem_gpio, GPIO_OUTPUT_ACTIVE);
	if (ret != 0) {
		printk("Failed to configure modem GPIO: %d\n", ret);
		err |= ERR_MODEM_GPIO;
	}
#ifdef CONFIG_CLOUD_SEND_4G
	printk("Starting modem\n");
	modem_pin_set(1);
	k_msleep(1000*15);
	if (uart_init() != 0) {
		err |= ERR_UART_SETUP;
	} else {
		modem_status_t modem_ret = initialize_modem();
		if(modem_ret != MODEM_SUCCESS) {
			printk("Modem initialization failed: %s (%d)\n", modem_status_to_string(modem_ret), modem_ret);
			err |= ERR_MODEM_INIT;
			err |= ERR_RTC_SET_TIME;
		} else {
			k_msleep(1000 * 12); // Module needs some time to activate internet connection and clock sync
			char time_str[32];
			size_t time_str_size = sizeof(time_str); 
			modem_get_time(time_str, time_str_size);
			if(strstr(time_str, "ERROR") == NULL){
				set_system_time(time_str, time);
				modem_power_off();
			} else {
				err |= ERR_RTC_SET_TIME;
			}
		}
	}
#endif
	if(err != ERR_NONE){
		print_errors(err);
	}

	//init_sensors();
	return err;
}

#if 0
int take_measurement(){
	json_data_t data;
	int tries = 0;
	while(1){
		printk("In loop\n");
		if(seen_count == RUUVITAG_COUNT){
			break;
		}
		if(tries >= 10){
			printk("Timeout\n");
			while(k_msgq_get(&json_msgq, &data, K_NO_WAIT) == 0){
				printk("Got: %s\n", data.mac);
				json_data_add(sensors, data.mac, data.temp, data.humidity, data.pressure);
			}
			batch_number++; // Partial or empty batches are still valid for record keeping
			return -1;
		} 
		tries++;
		k_msleep(1000);
	}
	while(k_msgq_get(&json_msgq, &data, K_NO_WAIT) == 0){
		printk("Got: %s\n", data.mac);
		json_data_add(sensors, data.mac, data.temp, data.humidity, data.pressure);
	}
	batch_number++; // Increment batch number for next upload
	return 0;
}

int find_sensor(SensorData *sensors, const char *mac){
	for(int i = 0; i < RUUVITAG_COUNT; i++){
		if(strcmp(sensors[i].mac, mac) == 0){
			return i;
		}
	}
	return -1;
}

void init_sensors(void){
	for(int i = 0; i < RUUVITAG_COUNT; i++){
		strncpy(sensors[i].mac, ruuvitag_devices[i], sizeof(sensors[i].mac));
		sensors[i].mac[sizeof(sensors[i].mac) - 1] = '\0';
	}
}

void json_data_add(SensorData *sensors, const char *mac, float temperature, float humidity, float pressure){
	int idx = find_sensor(sensors, mac);
	if(idx == -1){
		printk("Sensor not found\n"); // This should really trigger a separate pub/sub post upon next report somehow. All the printks should be replaced with a proper logging mechanism.
		return;
	}
	
	SensorData *sensor = &sensors[idx];
	if(sensor->measure_count >= MAX_MEASUREMENTS) return;

	int i = sensor->measure_count;
	sensor->temperature[i] = temperature;
	sensor->humidity[i] = humidity;
	sensor->pressure[i] = pressure;
	sensor->measure_count++; 
}
/*
char* json_data_string(int count) {
    int offset = 0;
    offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, "{");

    
    if (offset > 1) offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, ",");
    offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, "\"%s\":{", sensors[count].mac);

	// Quick and dirty batch number tracking
    offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, "\"batch_number\":%u,", batch_number);

	
    offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, "\"temperature\":[");
    for (int i = 0; i < sensors[count].measure_count; i++) {
        offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, "%.2f%s",
        sensors[count].temperature[i], (i < sensors[count].measure_count - 1) ? "," : "");
    }
	offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, "]"); // moved bracket termination here to prevent edge case issues with count=0

    offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, ",\"humidity\":[");
    for (int i = 0; i < sensors[count].measure_count; i++) {
        offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, "%.2f%s",
        sensors[count].humidity[i], (i < sensors[count].measure_count - 1) ? "," : "");
    }
	offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, "]");

    offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, ",\"pressure\":[");
    for (int i = 0; i < sensors[count].measure_count; i++) {
        offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, "%.2f%s",
        sensors[count].pressure[i], (i < sensors[count].measure_count - 1) ? "," : "");
    }
	offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, "]");

    offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, "}");
    offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, "}");

    return json_buf;
}
*/
// Helper function to format a float array as JSON
static int format_json_array(char *buf, size_t buf_size, int offset, 
                            const char *field_name, float *values, int count) {
    offset += snprintf(buf + offset, buf_size - offset, "\"%s\":[", field_name);
    for (int i = 0; i < count; i++) {
        offset += snprintf(buf + offset, buf_size - offset, "%.2f%s", // Maybe move the formatting into the field array later?
                          (double) values[i], (i < count - 1) ? "," : "");
    }
    offset += snprintf(buf + offset, buf_size - offset, "]");
    return offset;
}
//  should be a safe drop-in replacement of the old one
char* json_data_string(int count) {
    int offset = 0;
    offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, 
                      "{\"%s\":{\"batch_number\":%u,", sensors[count].mac, batch_number);
    // Array of field info
    struct {
        const char *name;
        float *data;
    } fields[] = {
        {"temperature", sensors[count].temperature},
        {"humidity", sensors[count].humidity},
        {"pressure", sensors[count].pressure}
    };
    for (int field = 0; field < 3; field++) {
        if (field > 0) offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, ",");
        offset = format_json_array(json_buf, sizeof(json_buf), offset, 
                                  fields[field].name, fields[field].data, sensors[count].measure_count);
    }
    offset += snprintf(json_buf + offset, sizeof(json_buf) - offset, "}}");
    return json_buf;
}


int send_data(void){
	const char *access_token = cloud_request_access_token();
	for(int i = 0; i < RUUVITAG_COUNT; i++){
		const char *data = json_data_string(i);
		int result = cloud_publish(access_token, data);

		if(result != MODEM_SUCCESS){
			return result;
		}
	}
	//int result = cloud_publish(access_token, data);
	k_free((void *) access_token);
	
	return 0;
}

int send_scheduled_data(void){
	int ret = 0;
	
	ret = send_data();
	if (ret != 0){
		printk("ERROR: Failed to publish data\n");
	} else {
		clean_data();
	}
	
	return ret;
}

void clean_data(){
	for (int i = 0; i < RUUVITAG_COUNT; i++) {
		batch_number = 0; // Reset batch number after successful send
        memset(sensors[i].temperature, 0, sizeof(sensors[i].temperature));
        memset(sensors[i].humidity, 0, sizeof(sensors[i].humidity));
        memset(sensors[i].pressure, 0, sizeof(sensors[i].pressure));
        sensors[i].measure_count = 0;
    }
}
#endif

int startup_modem(void){

	modem_pin_set(1);
	k_msleep(1000 * 20);

	modem_status_t modem_ret = initialize_modem();
	if(modem_ret != MODEM_SUCCESS) {
		printk("Modem initialization failed: %s (%d)\n", modem_status_to_string(modem_ret), modem_ret);

		printk("Trying to initialize Modem again...\n");
        int tries = 0;
        while(tries <= 5){
            k_msleep(1000 * 10);
            printk(".\n");
            if(initialize_modem() == MODEM_SUCCESS){
                printk("Success\n");
                break;
            }
            tries++;
        }
        if(tries > 5){
			printk("Failed\n");
			return -1;
		}
	}
	k_msleep(1000 * 15); // Module needs some time to activate internet connection
	
	return 0;
}

void modem_power_off(void){
	modem_shutdown();
	k_msleep(1000 * 10);
	modem_pin_set(0);
}

void modem_pin_set(int state){
	gpio_pin_set_dt(&modem_gpio, state);
}

bool gpio_status(void){
	return device_is_ready(modem_gpio.port);
}

