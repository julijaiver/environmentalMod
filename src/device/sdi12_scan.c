#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sdi12_scan);

#include <stdio.h>
#include <time.h>

#include "common.h"
#include "device.h"
#include "sdi12.h"
#include "sdi12_scan.h"
#include "data_types.h"
#include "data_queue.h"

struct sdi12_sensors {
    char addr;
    struct sensor_data data;
};

static int sdi12_scan_sensors(struct sdi12_sensors *sn, int max_sensors) 
{
    int sid = 0;
    for(int i = 0; i <= 9 && sid < max_sensors; ++i) {
		char response[64];
        char cmd[] = "0I!";
        cmd[0] = '0' + i;
		sdi12_cmd(cmd, true);

        // (echo 3) + 1 + 2 + 8 + 6 + 3 + (optional SN up to 13 chars) + 2 (CR+LF) --> min 25 chars
		if(sdi12_wait_for(response, sizeof(response), NULL) >= 25) {
            struct known_sensors {
                const char *identifier;
                int type;
            } known[] = {
                { "METER   TER12 ", TYPE_TEROS12 },
                { "METER   SLYX14", TYPE_SOLYX14 },
                { NULL, TYPE_UNKNOWN }
            }; 
                
			response[strcspn(response, "\r\n")] = 0;

            for( struct known_sensors *kn = known; kn->identifier != NULL; ++kn) {
                char *str = strstr(response, kn->identifier);
                if (str != NULL) {
                    strncpy(sn[sid].data.id, str + 17, sizeof(sn[sid].data.id)); // copy string starting from (optional) serial number
                    sn[sid].data.id[sizeof(sn[sid].data.id) - 1] = 0;           // ensure termination
                    sn[sid].data.type = kn->type;
                    sn[sid].addr = cmd[0];
                    ++sid;
                    break;
                }
            
            }
		}

    }

    return sid;
}


void sdi12_scan_thread(void *arg0, void *arg1, void *arg2)
{
    char response[128];
    struct sdi12_sensors sensors[3] = { { 0 }, { 0 }, { 0 } };

    sdi12_init();

    BOOT_WAIT();

    int sensor_count = sdi12_scan_sensors(sensors, 3);

    CLOCK_WAIT();


    while (true)
    {
        for( int i = 0; i < sensor_count; ++i) {
            if(sensors[i].data.type == TYPE_TEROS12)
            {
                char cmd[] = "0R0!";
                cmd[0] = sensors[i].addr;
                char expect[] = "0R0!0";
                expect[0] = cmd[0];
                expect[4] = cmd[0];
                sdi12_cmd(cmd, true);
                if (sdi12_wait_for(response, sizeof(response), expect) > 0)
                {
                    // extract values
                    char *rsp = strstr(response, expect);
                    if (!rsp)
                    {
                        LOG_ERR("No match found");
                        continue; // should not happens since wait_for found a match
                    }
                    int rv = sscanf(rsp + strlen(expect), "%f%f%f", &sensors[i].data.teros.vwc, &sensors[i].data.teros.temp, &sensors[i].data.teros.ec);
                    if (rv == 3)
                    {
                        // the id is set during init, just set timestamp
                        sensors[i].data.timestamp = time(NULL);
                        if (data_put(&sensors[i].data) < 0)
                        {
                            LOG_ERR("Failed to queue data");
                        }
                    }
                    else
                    {
                        LOG_ERR("Data parse failed");
                    }
                }
                else
                {
                    LOG_INF("Response timeout");
                }
            } else if(sensors[i].data.type == TYPE_SOLYX14) {
                char cmd[] = "0XR0!";
                cmd[0] = sensors[i].addr;
                char expect[] = "0XR0!0";
                expect[0] = cmd[0];
                expect[5] = cmd[0];
                sdi12_cmd(cmd, true);
                if (sdi12_wait_for(response, sizeof(response), expect) > 0)
                {
                    // extract values
                    char *rsp = strstr(response, expect);
                    if (!rsp)
                    {
                        LOG_ERR("No match found");
                        continue; // should not happens since wait_for found a match
                    }
                    int rv = sscanf(rsp + strlen(expect), "%f%f%f", &sensors[i].data.solyx.perm, &sensors[i].data.solyx.temp, &sensors[i].data.solyx.ec);
                    if (rv == 3)
                    {
                        // with single sensor the id is set during init, just set timestamp
                        sensors[i].data.timestamp = time(NULL);
                        if (data_put(&sensors[i].data) < 0)
                        {
                            LOG_ERR("Failed to queue data");
                        }
                    }
                    else
                    {
                        LOG_ERR("Data parse failed");
                    }
                }
                else
                {
                    LOG_INF("Response timeout");
                }
    
                
            }  
        }

        k_msleep(MEASURE_CYCLE_SLEEP);
    }
}
