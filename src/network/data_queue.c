#include <zephyr/kernel.h>
#include <stdio.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cloud_send, CONFIG_MODEM_MODULES_LOG_LEVEL);

#include "device.h"
#include "modem.h"
#include "cloud.h"
#include "data_queue.h"

static void cloud_send(void *p1, void *p2, void *p3);

#define CLOUD_SEND_THREAD_PRIORITY 7
#define CLOUD_WAKEUP_EVENT 1
#define CLOUD_WAKEUP_PERIOD 60

K_MSGQ_DEFINE(transmit_queue, sizeof(struct sensor_data), DATA_QUEUE_LENGTH, 1);

K_THREAD_DEFINE(cloud_send_thread, 8192, cloud_send, NULL, NULL, NULL, CLOUD_SEND_THREAD_PRIORITY, 0, 0);

K_EVENT_DEFINE(cloud_events);

K_TIMER_DEFINE(cloud_send_timer, cloud_send_notify, NULL);

int data_put(struct sensor_data *data)
{
    int rc = k_msgq_put(&transmit_queue, data, K_MSEC(300));
    int retry = 10;

    switch (rc)
    {
    case -ENOMSG:
        // can't send at all
        break;
    case -EAGAIN:
        // signal transmit task to wake up and then try again up to 10 times
        cloud_send_notify(NULL);
        do
        {
            rc = k_msgq_put(&transmit_queue, data, K_MSEC(30000));
            --retry;
            // write log message
        } while (retry > 0 && rc == -EAGAIN);
        break;
    case 0:
        // success
        break;
    default:
        // should never happen...
        break;
    }
    return rc;
}

void cloud_send_notify(struct k_timer *timer)
{
    ARG_UNUSED(timer);

    k_event_post(&cloud_events, CLOUD_WAKEUP_EVENT);
}

static int ruuvi_tag_to_json(struct sensor_data *data, char *json_buf, int size)
{
    return snprintf(json_buf, size,
             "{\"mac\":\"%s\",\"ts\":%ld,\"t\":%.2f,\"h\":%.2f,\"p\":%.2f,\"bv\":%.2f}",
             data->id, (long int)data->timestamp,
             (double)data->ruuvi.temperature, (double)data->ruuvi.humidity,
             (double)data->ruuvi.pressure, (double)data->ruuvi.bat_voltage);
};

static int teros12_to_json(struct sensor_data *data, char *json_buf, int size)
{
    return snprintf(json_buf, size,
             "{\"id\":\"%s\",\"ts\":%ld,\"vwc\":%.2f,\"t\":%.2f,\"ec\":%.2f}",
             data->id, (long int)data->timestamp,
             (double)data->teros.vwc, (double)data->teros.temp, (double)data->teros.ec);
};

#define JSON_MARGIN  128

static void cloud_send(void *p1, void *p2, void *p3)
{
    struct sensor_data data;
    k_timer_start(&cloud_send_timer, K_MINUTES(CLOUD_WAKEUP_PERIOD), K_MINUTES(CLOUD_WAKEUP_PERIOD));

    while (true)
    {
        int fail_count = 0;
        const char *access_token = NULL;
        k_event_wait(&cloud_events, CLOUD_WAKEUP_EVENT, true, K_FOREVER);
        LOG_INF("wakeup");
        if (startup_modem() == 0)
        {
            access_token = cloud_request_access_token();

            while (access_token && fail_count < CLOUD_SEND_RETRY_COUNT && k_msgq_num_used_get(&transmit_queue) > 0)
            {
                static char json_msg[2048];
                int msg_count = 0;
                int json_pos = 0;
                json_pos = snprintf(json_msg, sizeof(json_msg), "{\"measurements\":[");
                // peek first and remove only after successfull transmit
                while (json_pos + JSON_MARGIN < sizeof(json_msg) && k_msgq_peek_at(&transmit_queue, &data, msg_count) == 0)
                {
                    LOG_INF("%s %u", data.id, (unsigned int)data.timestamp);
                    if(msg_count > 0) {
                        strcat(json_msg + json_pos, ",");
                        ++json_pos;
                    }
                    switch (data.type)
                    {
                    case TYPE_RUUVI_TAG:
                        json_pos += ruuvi_tag_to_json(&data, json_msg + json_pos, sizeof(json_msg) - json_pos);
                        break;
                    case TYPE_TEROS12:
                        json_pos += teros12_to_json(&data, json_msg + json_pos, sizeof(json_msg) - json_pos);
                        break;
                    default:
                        LOG_INF("Unknown data type in queue");
                    }
                    ++msg_count;
                }
                json_pos += snprintf(json_msg + json_pos, sizeof(json_msg)-json_pos, "]}");
                LOG_INF("JSON: %d, %d", msg_count, json_pos);
                int result = cloud_publish(access_token, json_msg);

                if (result == MODEM_SUCCESS)
                {
                    fail_count = 0;
                    // remove message from queue after transmit - should always succeed because we already peeked the message
                    while(msg_count > 0) 
                    {
                        if(k_msgq_get(&transmit_queue, &data, K_NO_WAIT) == 0)
                            --msg_count;
                        else
                            LOG_ERR("No message after succesfull peek");
                    }
                }
                else
                {
                    ++fail_count;
                    LOG_INF("Send failed: %d", fail_count);
                }
            }

            k_free((void *)access_token);
        }

        modem_power_off();
    }
}
