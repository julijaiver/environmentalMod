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
#include "cJSON_helper.h"

static const struct gpio_dt_spec modem_gpio = GPIO_DT_SPEC_GET(MODEM_PIN_NODE, modem_pin_gpios);

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

	// Initialize GPIO for modem control
	if (!device_is_ready(modem_gpio.port)) {
		printk("Modem GPIO device not ready\n");
		err |= ERR_MODEM_INIT;
	}

	int ret = gpio_pin_configure_dt(&modem_gpio, GPIO_OUTPUT_ACTIVE);
	if (ret != 0) {
		printk("Failed to configure modem GPIO: %d\n", ret);
		err |= ERR_MODEM_INIT;
	}
	printk("Starting modem\n");
	modem_pin_set(1);
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
	if(result != MODEM_SUCCESS){
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
		} else {
			cJSON_free(json_string);
		}
	}
	else{
		ret = -1;
		printk("ERROR: Failed to get JSON string\n");
	}
	
	return ret;
}

int startup_modem(void){

	// TODO: control power transistor with gpio
	modem_pin_set(1);
	uint8_t err = ERR_NONE;
	
	k_msleep(1000 * 12);
	// TODO: initialize modem
	modem_status_t modem_ret = initialize_modem();
	if(modem_ret != MODEM_SUCCESS) {
		printk("Modem initialization failed: %s (%d)\n", modem_status_to_string(modem_ret), modem_ret);
		err |= ERR_MODEM_INIT;

		printk("Trying to initialize Modem again...\n");
        int tries = 0;
        while(tries <= 5){
            k_msleep(1000 * 10);
            printk(".\n");
            if(initialize_modem() == MODEM_SUCCESS){
                printk("Success\n");
                err &= ~ERR_MODEM_INIT;
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
	modem_pin_set(0);
}

void modem_pin_set(int state){
	gpio_pin_set_dt(&modem_gpio, state);
}