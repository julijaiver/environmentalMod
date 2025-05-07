// Standard libraries
#include <stdio.h>
#include <stdlib.h>

// RTOS
#include <zephyr/kernel.h>

// Includes
#include "errors.h"
#include "uart.h"
#include "modem.h"
#include "ruuvitag.h"
#include "realtime.h"

void print_errors(uint8_t err){
    if(err == ERR_NONE) printk("Initilization successfull!\n");
    if(err & ERR_UART_SETUP) printk("UART setup error!\n");
    if(err & ERR_MODEM_INIT) printk("Modem setup error\n");
    if(err & ERR_BLUETOOTH_SETUP) printk("Bluetooth setup error!\n");
    if(err & ERR_RTC_SET_TIME) printk("RTC set time error!\n");
}

void handle_errors(uint8_t *err, struct tm *time) {
    int tries = 0;

    if(*err & ERR_UART_SETUP){
        printk("Trying to initialize UART again...\n");
        while(tries <= 5){
            k_msleep(1000 * 10);
            printk(".\n");
            if(uart_init() == 0){
                printk("Success\n");
                *err &= ~ERR_UART_SETUP;
                break;
            }
            tries++;
        }
        if(tries > 5) printk("Failed\n");
    } 
    if(*err & ERR_MODEM_INIT) {
        printk("Trying to initialize Modem again...\n");
        tries = 0;
        while(tries <= 5){
            k_msleep(1000 * 10);
            printk(".\n");
            if(initialize_modem() == MODEM_SUCCESS){
                printk("Success\n");
                *err &= ~ERR_MODEM_INIT;
                char time_str[32];
                size_t time_str_size = sizeof(time_str); 
                modem_get_time(time_str, time_str_size);
                if(strstr(time_str, "ERROR") == NULL){
                    set_system_time(time_str, time);
                    *err &= ~ERR_RTC_SET_TIME;
                }
                break;
            }
            tries++;
        }
        if(tries > 5) printk("Failed\n");

    } 
    if(*err & ERR_BLUETOOTH_SETUP) {
        printk("Trying to initialize Bluetooth again...\n");
        tries = 0;
        while(tries <= 5){
            k_msleep(1000 * 10);
            printk(".\n");
            
            if(activate_bluetooth() == 0){
                printk("Success\n");
                *err &= ~ERR_BLUETOOTH_SETUP;
                break;
            }
            
            tries++;
        }
        if(tries > 5) printk("Failed\n");
    }
}