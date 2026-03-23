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

static void ruuvi_tag_to_json(struct sensor_data *data, char *json_buf, int size)
{
    snprintf(json_buf, size,
             "{\"mac\":\"%s\",\"ts\":%ld,\"t\":%.2f,\"h\":%.2f,\"p\":%.2f,\"bv\":%.2f}",
             data->id, (long int)data->timestamp,
             (double)data->ruuvi.temperature, (double)data->ruuvi.humidity, 
             (double)data->ruuvi.pressure, (double) data->ruuvi.bat_voltage);
};

static void teros12_to_json(struct sensor_data *data, char *json_buf, int size)
{
    snprintf(json_buf, size,
             "{\"id\":\"%s\",\"ts\":%ld,\"vwc\":%.2f,\"t\":%.2f,\"ec\":%.2f}",
             data->id, (long int)data->timestamp,
             (double)data->teros.vwc, (double)data->teros.temp, (double)data->teros.ec);
};

static void cloud_send(void *p1, void *p2, void *p3)
{
    struct sensor_data data;
    k_timer_start(&cloud_send_timer, K_MINUTES(CLOUD_WAKEUP_PERIOD), K_MINUTES(CLOUD_WAKEUP_PERIOD));

    while (true)
    {
        int fail_count = 0;
        k_event_wait(&cloud_events, CLOUD_WAKEUP_EVENT, true, K_FOREVER);
        LOG_INF("wakeup");
#if 0
        while (k_msgq_get(&transmit_queue, &data, K_NO_WAIT) == 0)
        {
            static char json_msg[512];
            teros12_to_json(&data, json_msg, sizeof(json_msg));
            LOG_INF("%s", json_msg);
        }
#else
        if (startup_modem() == 0)
        {
            const char *access_token = cloud_request_access_token();
            // peek first and remove only after successfull transmit
            while (fail_count < CLOUD_SEND_RETRY_COUNT && k_msgq_peek(&transmit_queue, &data) == 0)
            {
                LOG_INF("%s %u", data.id, (unsigned int)data.timestamp);
                static char json_msg[512];

                switch (data.type)
                {
                case TYPE_RUUVI_TAG:
                    ruuvi_tag_to_json(&data, json_msg, sizeof(json_msg));
                    break;
                case TYPE_TEROS12:
                    teros12_to_json(&data, json_msg, sizeof(json_msg));
                    break;
                default:
                    json_msg[0] = 0;
                }
                LOG_INF("JSON: %s", json_msg);
                int result = cloud_publish(access_token, json_msg);

                if (result == MODEM_SUCCESS)
                {
                    fail_count = 0;
                    // remove message from queue after transmit - should always succeed because we already peeked the message
                    if (k_msgq_get(&transmit_queue, &data, K_NO_WAIT) != 0)
                    {
                        LOG_ERR("No message after succesfull peek");
                    }
                }
                else {
                    ++fail_count;
                    LOG_INF("Send failed: %d", fail_count);
                }
            }

            k_free((void *)access_token);
        }

        modem_power_off();
#endif
    }
}
