#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>

#include <stdio.h>
#include <time.h>

#include "device.h"
#include "sdi12.h"
#include "sdi12_scan.h"
#include "data_types.h"
#include "data_queue.h"


void sdi12_scan_thread(void *arg0, void *arg1,  void *arg2)
{
    char response[128];
    struct sensor_data data = { .type = TYPE_TEROS12, .timestamp = 0, .id = "Unknown SDI-12 device" };

    sdi12_init();

    sdi12_cmd("0I!", true);
    if (sdi12_wait_for(response, sizeof(response), "0I!") > 0)
    {
        // record sensor id
        char *str = strstr(response, "METER   TER12");
        if(str) {
            strncpy(data.id, str + 8, sizeof(data.id)); // copy string starting from "TER"
            data.id[sizeof(data.id) -1] = 0; // ensure termination
            data.id[strcspn(data.id, " ")] = '-'; // replace space with dash
            data.id[strcspn(data.id, "\n\r")] = 0; // remove CR/LF
        }
    }

    while (true)
    {
        sdi12_cmd("0R0!", true);
        if (sdi12_wait_for(response, sizeof(response), "0R0!0") > 0)
        {
            // extract values
            char *rsp = strstr(response, "0R0!0");
            if(!rsp) continue; // should not happens since wait_for found a match

            int rv = sscanf(rsp + 5, "%f%f%f", &data.teros.vwc, &data.teros.temp, &data.teros.ec);
            if(rv == 3) {
                data.timestamp = time(NULL);
                // teros 12
                if(data_put(&data) < 0) {
                    // error
                }
                
            }
#if 0
            else if(rv == 2) {
                // teros 11 - not implemented yet
            }
#endif
        }

        k_msleep(MEASURE_CYCLE_SLEEP);
    }
}
