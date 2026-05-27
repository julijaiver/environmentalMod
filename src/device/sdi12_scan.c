#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sdi12_scan);

#include <stdio.h>
#include <time.h>
#include <math.h>

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

static void read_teros_solyx_sn(char *dst, size_t size, const char *src) 
{
    // src points to METER so the optional data starts from str + 17
    //0I!014METER   SLYX14100SX14000000208   
    if(strlen(src) < 17) {
        *dst = 0;
        return;
    }
    strncpy(dst, src + 17, size); // copy string starting from (optional) serial number
    dst[size - 1] = 0;  // ensure termination
}


static void read_solinst_sn(char *dst, size_t size, const char *src) 
{
    // src points to SOLINST so the optional data starts from str + 17
    //0I!013SOLINST M10/C XD11.006 1090717
    if(size < 4 || strlen(src) < 17) {
        *dst = 0;
        return;
    }
    strcpy(dst, "SOL"); // prefix SN with char identifier to avoid possible SN conflict
    size -= 4;
    src += 17;
    // solinst optional field contains firmware revision (5 chars) before serial number
    for(int i = 0; *src && i < 5; ++i) ++src;
    // there may be leading space to skip
    while(*src == ' ') ++src;

    strncat(dst, src, size); // append (optional) serial number
    //dst[size - 1] = 0;  // ensure termination

}


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
                void (*sn_reader)(char *, size_t, const char *);
            } known[] = {
                { "METER   TER12 ", TYPE_TEROS12, read_teros_solyx_sn },
                { "METER   SLYX14", TYPE_SOLYX14, read_teros_solyx_sn },
                { "SOLINST ", TYPE_SOLINST, read_solinst_sn }, 
                { NULL, TYPE_UNKNOWN }
            }; 
            
 			response[strcspn(response, "\r\n")] = 0;

            for( struct known_sensors *kn = known; kn->identifier != NULL; ++kn) {
                char *str = strstr(response, kn->identifier);
                if (str != NULL) {
                    kn->sn_reader(sn[sid].data.id, sizeof(sn[sid].data.id), str); 
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
        k_event_wait(&envisens_events, TAKE_SAMPLE_EVENT, true, K_FOREVER);

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
            } 
            
            else if(sensors[i].data.type == TYPE_SOLYX14) {
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
                    int rv = sscanf(rsp + strlen(expect), "%f%f%f", &sensors[i].data.solyx.epsr, &sensors[i].data.solyx.temp, &sensors[i].data.solyx.bulk_ec);
                    if (rv == 3)
                    {
                        // with single sensor the id is set during init, just set timestamp
                        sensors[i].data.timestamp = time(NULL);
                        //calculating vwc and pw_ec values from raw
                        float epsp = 80.3f - 0.37f * (sensors[i].data.solyx.temp - 20.0f);
                        sensors[i].data.solyx.vwc = 0.0985f * sqrtf(sensors[i].data.solyx.epsr) - 0.159f;
                        sensors[i].data.solyx.pw_ec = (epsp * sensors[i].data.solyx.bulk_ec) / (sensors[i].data.solyx.epsr - 4.1f);
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
            //in the datasheet, R commands are not specified, only M and D?
            //this could be for solinst levelogger
            #if 1
            else if(sensors[i].data.type == TYPE_SOLINST) {
                char cmd_m[] = "0M!";
                cmd_m[0] = sensors[i].addr;
                char expect[] = "0M!0";
                expect[0] = cmd_m[0];
                expect[3] = cmd_m[0];
                sdi12_cmd(cmd_m, true);
                if (sdi12_wait_for(response, sizeof(response), expect) > 0)
                {
                    char *rsp = strstr(response, expect);
                    if (rsp)
                    {
                        int wait = 0;
                        int val_num = 0;
                        //get wait time and num of vals
                        int read_val = sscanf(rsp +strlen(expect), "%3d%1d", &wait, &val_num);
                        if (read_val == 2 && val_num >= 2)
                        {
                            k_sleep(K_SECONDS(wait +1));//just in case +1
                            // get data with D
                            char cmd_d[] = "0D0!";
                            cmd_d[0] = sensors[i].addr;
                            char expect_d[] = "0D0!0";
                            expect_d[0] = cmd_d[0];
                            expect_d[4] = cmd_d[0];
                            sdi12_cmd(cmd_d, true);
                            if (sdi12_wait_for(response, sizeof(response), expect_d) > 0)
                            {
                                char *rsp_d = strstr(response, expect_d);
                                if (rsp_d)
                                {
                                    int rv = sscanf(rsp_d + strlen(expect_d), "%f%f", &sensors[i].data.solinst.temp, &sensors[i].data.solinst.level);
                                    if (rv == 2)
                                    {
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
                                } else
                                {
                                    LOG_ERR("aD0 response timeout");
                                }
                            }
                        }
                    }
                } else {
                    LOG_ERR("aM! response timeout");
                }
            }
            #endif
        }
    }
}
