#include <zephyr/kernel.h>
#include <stdio.h>
#include <math.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cloud_send);

#include "device.h"
#include "modem.h"
#include "cloud.h"
#include "data_queue.h"
#include "common.h"
#include "lora.h"

static void cloud_send(void *p1, void *p2, void *p3);

#define CLOUD_SEND_STACK_SIZE 16384 // 8192
#define CLOUD_SEND_THREAD_PRIORITY 7
#define CLOUD_WAKEUP_PERIOD 10

// size of max len for msghex
#define RUUVI_ID_LEN 6

K_MSGQ_DEFINE(transmit_queue, sizeof(struct sensor_data), DATA_QUEUE_LENGTH, 1);

K_THREAD_DEFINE(cloud_send_thread, CLOUD_SEND_STACK_SIZE, cloud_send, NULL, NULL, NULL, CLOUD_SEND_THREAD_PRIORITY, 0, 0);

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

#ifdef CONFIG_CLOUD_SEND_4G
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
    double raw = data->teros.vwc;
    double vwc = 6.771e-10 * pow(raw, 3) - 5.105e-6 * pow(raw, 2) + 1.302e-2 * raw - 10.848;

    return snprintf(json_buf, size,
                    "{\"id\":\"%s\",\"ts\":%ld,\"vwc\":%.2f,\"t\":%.2f,\"ec\":%.2f,\"raw\":%.2f}",
                    data->id, (long int)data->timestamp,
                    vwc, (double)data->teros.temp, (double)data->teros.ec, raw);
};


static int solyx14_to_json(struct sensor_data *data, char *json_buf, int size)
{
    return snprintf(json_buf, size,
                    "{\"id\":\"%s\",\"ts\":%ld,\"perm\":%.2f,\"t\":%.2f,\"ec\":%.2f}",
                    data->id, (long int)data->timestamp,
                    (double)data->solyx.epsr, (double)data->solyx.temp, (double)data->solyx.bulk_ec);
};
#endif

static int get_message_len(const struct sensor_data *data)
{
    switch (data->type)
    {
        // format: type(1b) + id_len(1b) + id + timestamp(4b) + data
    case TYPE_RUUVI_TAG:
        return 1 + 1 + RUUVI_ID_LEN + sizeof(uint32_t) +  4 * sizeof(float);
    case TYPE_TEROS12:
        return 1 + 1 + strlen(data->id) + sizeof(uint32_t) + 3 * sizeof(float);
    case TYPE_SOLYX14:
        return 1 + 1 + strlen(data->id) + sizeof(uint32_t) + 5 * sizeof(float);
    case TYPE_SOLINST:
        return 1 + 1 + strlen(data->id) + sizeof(uint32_t) + 2 * sizeof(float);
    default:
        return -1;
    }
}

// function for payload construction
static int serialize_payload(uint8_t *buf, const struct sensor_data *data)
{
    int pos = 0;
    uint8_t id_len;
    uint32_t time = (uint32_t)data->timestamp;
    //add type and timestamp
    buf[pos++] = (uint8_t)data->type;

    //for mac addr it's different than sdi12 sensors
    if (data->type == TYPE_RUUVI_TAG)
    {
        id_len = RUUVI_ID_LEN;
        buf[pos++] = id_len;
        sscanf(data->id, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
               &buf[pos], &buf[pos + 1], &buf[pos + 2], &buf[pos + 3], &buf[pos + 4], &buf[pos + 5]);
        pos += id_len;
    }
    else
    {
        id_len = (uint8_t)strlen(data->id);
        buf[pos++] = id_len;
        memcpy(buf + pos, data->id, id_len);
        pos += id_len;
    }

    memcpy(buf + pos, &time, sizeof(time));
    pos += sizeof(time);

    //switch for different sensor data 
    switch (data->type)
    {
    case TYPE_RUUVI_TAG:
        memcpy(buf + pos, &data->ruuvi.temperature, sizeof(float));
        pos += sizeof(float);
        memcpy(buf + pos, &data->ruuvi.humidity, sizeof(float));
        pos += sizeof(float);
        memcpy(buf + pos, &data->ruuvi.pressure, sizeof(float));
        pos += sizeof(float);
        memcpy(buf + pos, &data->ruuvi.bat_voltage, sizeof(float));
        pos += sizeof(float);
        break;
    case TYPE_TEROS12:
        memcpy(buf + pos, &data->teros.vwc, sizeof(float));
        pos += sizeof(float);
        memcpy(buf + pos, &data->teros.temp, sizeof(float));
        pos += sizeof(float);
        memcpy(buf + pos, &data->teros.ec, sizeof(float));
        pos += sizeof(float);
        break;
    case TYPE_SOLYX14:
        memcpy(buf + pos, &data->solyx.epsr, sizeof(float));
        pos += sizeof(float);
        memcpy(buf + pos, &data->solyx.temp, sizeof(float));
        pos += sizeof(float);
        memcpy(buf + pos, &data->solyx.bulk_ec, sizeof(float));
        pos += sizeof(float);
        memcpy(buf + pos, &data->solyx.vwc, sizeof(float));
        pos += sizeof(float);
        memcpy(buf + pos, &data->solyx.pw_ec, sizeof(float));
        pos += sizeof(float);
        break;
    case TYPE_SOLINST:
        memcpy(buf + pos, &data->solinst.temp, sizeof(float));
        pos += sizeof(float);
        memcpy(buf + pos, &data->solinst.level, sizeof(float));
        pos += sizeof(float);
        break;
    default:
        // type error
        return -1;
    }
    return pos; //payload size
}

int send_log_msg(struct msg_log *msg)
{
    uint8_t buf[DR1_LEN];
    buf[0] = (uint8_t)msg->type;

    if (msg->type == TYPE_LOG_MSG_TXT) 
    {
        int len = strnlen(msg->txt_msg, MSG_TXT_MAX_LEN);
        memcpy(buf + 1, msg->txt_msg, len);
        return lora_queue_payload(buf, 1 + len);
    } else if (msg->type == TYPE_LOG_MSG_INT)
    {
        uint8_t count = msg->int_msg.count;
        buf[1] = msg->int_msg.id;
        memcpy(buf + 2, msg->int_msg.int_vals, count * sizeof(uint32_t));
        return lora_queue_payload(buf, 2 + count * sizeof(uint32_t));
    }
    return -1;
}

// cloud send is based on assumption that a single measurement is less than this size
#define JSON_ELEMENT_MAX_SIZE 256
// margin for closing the JSON array of measurements and comma between array elements
#define JSON_MARGIN 16

#ifdef CONFIG_CLOUD_SEND_4G
static void cloud_send(void *p1, void *p2, void *p3)
{
    struct sensor_data data;

    BOOT_WAIT();

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
                while (json_pos + JSON_ELEMENT_MAX_SIZE + JSON_MARGIN < sizeof(json_msg) && k_msgq_peek_at(&transmit_queue, &data, msg_count) == 0)
                {
                    LOG_INF("%s %u", data.id, (unsigned int)data.timestamp);
                    if (msg_count > 0)
                    { // add comma before element if this is not the first element
                        strcat(json_msg + json_pos, ",");
                        ++json_pos;
                    }
                    switch (data.type)
                    {
                    case TYPE_RUUVI_TAG:
                        json_pos += ruuvi_tag_to_json(&data, json_msg + json_pos, JSON_ELEMENT_MAX_SIZE);
                        break;
                    case TYPE_TEROS12:
                        json_pos += teros12_to_json(&data, json_msg + json_pos, JSON_ELEMENT_MAX_SIZE);
                        break;
                    case TYPE_SOLYX14:
                        json_pos += solyx14_to_json(&data, json_msg + json_pos, JSON_ELEMENT_MAX_SIZE);
                        break;
                    default:
                        LOG_INF("Unknown data type in queue");
                    }
                    ++msg_count;
                }
                json_pos += snprintf(json_msg + json_pos, sizeof(json_msg) - json_pos, "]}");
                LOG_INF("JSON: %d, %d", msg_count, json_pos);
                int result = cloud_publish(access_token, json_msg);

                if (result == MODEM_SUCCESS)
                {
                    fail_count = 0;
                    // remove message from queue after transmit - should always succeed because we already peeked the message
                    while (msg_count > 0)
                    {
                        if (k_msgq_get(&transmit_queue, &data, K_NO_WAIT) == 0)
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
            if (access_token == NULL)
                LOG_INF("No access token");

            k_free((void *)access_token);
        }

        modem_power_off();
    }
}
#endif

#if 0
static void cloud_send(void *p1, void *p2, void *p3)
{
    struct sensor_data data;

    BOOT_WAIT();

    k_timer_start(&cloud_send_timer, K_MINUTES(CLOUD_WAKEUP_PERIOD), K_MINUTES(CLOUD_WAKEUP_PERIOD));

    while (true)
    {
        int fail_count = 0;
        k_event_wait(&cloud_events, CLOUD_WAKEUP_EVENT, true, K_FOREVER);
        LOG_INF("wakeup");
        // with LoRa we should send one message at a time
        while (fail_count < CLOUD_SEND_RETRY_COUNT && k_msgq_num_used_get(&transmit_queue) > 0)
        {
            static char json_msg[2048];
            int msg_count = 0;
            int json_pos = 0;
            json_pos = snprintf(json_msg, sizeof(json_msg), "{\"measurements\":[");
            // peek first and remove only after successfull transmit
            while (json_pos + JSON_ELEMENT_MAX_SIZE + JSON_MARGIN < sizeof(json_msg) && k_msgq_peek_at(&transmit_queue, &data, msg_count) == 0)
            {
                LOG_INF("%s %u", data.id, (unsigned int)data.timestamp);
                if (msg_count > 0)
                { // add comma before element if this is not the first element
                    strcat(json_msg + json_pos, ",");
                    ++json_pos;
                }
                switch (data.type)
                {
                case TYPE_RUUVI_TAG:
                    json_pos += ruuvi_tag_to_json(&data, json_msg + json_pos, JSON_ELEMENT_MAX_SIZE);
                    break;
                case TYPE_TEROS12:
                    json_pos += teros12_to_json(&data, json_msg + json_pos, JSON_ELEMENT_MAX_SIZE);
                    break;
                case TYPE_SOLYX14:
                    json_pos += solyx14_to_json(&data, json_msg + json_pos, JSON_ELEMENT_MAX_SIZE);
                    break;
                default:
                    LOG_INF("Unknown data type in queue");
                }
                ++msg_count;
            }
            json_pos += snprintf(json_msg + json_pos, sizeof(json_msg) - json_pos, "]}");
            LOG_INF("JSON: %d, %d", msg_count, json_pos);
            int result = MODEM_SUCCESS; // this where we send to cloud
            printk("%s\n", json_msg);

            if (result == MODEM_SUCCESS)
            {
                fail_count = 0;
                // remove message from queue after transmit - should always succeed because we already peeked the message
                while (msg_count > 0)
                {
                    if (k_msgq_get(&transmit_queue, &data, K_NO_WAIT) == 0)
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
    }
}

#endif

#ifdef CONFIG_CLOUD_SEND_LORA

//test data 
static void test_data_timer_cb(struct k_timer *timer) {
    static float perm = 0.1;
    static float temp = 20.0;
    static float ec = 0.5;
    static float vwc = 0.2;
    static float pw_ec = 0.123;
    struct sensor_data test = {
        .type = TYPE_SOLYX14,
        .timestamp = time(NULL),
        .id = "ASF126DHSOLYX140000012",
        .solyx = {
            .epsr = perm,
            .temp = temp,
            .bulk_ec = ec,
            .vwc = vwc,
            .pw_ec = pw_ec
        } 
    };
    perm += 0.1f;
    temp += 0.5f;
    ec += 0.2f;
    vwc += 0.05f;
    pw_ec += 0.001f;
    data_put(&test);
}
K_TIMER_DEFINE(test_data_timer, test_data_timer_cb, NULL);

static void cloud_send(void *p1, void *p2, void *p3)
{
    struct sensor_data data;
    k_timer_start(&cloud_send_timer, K_MINUTES(CLOUD_WAKEUP_PERIOD), K_MINUTES(CLOUD_WAKEUP_PERIOD));
    //timer for populating test data - to be removed
    BOOT_WAIT();
    //k_timer_start(&test_data_timer, K_SECONDS(0), K_MINUTES(30)); 

    //accumulating statistic avg
    static uint32_t total_payload_len = 0;
    static uint32_t day_msg_count = 0;
    static uint32_t total_send_count = 0;
    static int last_day = -1;

    while (true)
    {
        int fail_count = 0;
        k_event_wait(&cloud_events, CLOUD_WAKEUP_EVENT, true, K_FOREVER);
        LOG_INF("wakeup");
                                                                                                            
        LOG_INF("Queue items: %d", k_msgq_num_used_get(&transmit_queue));   
        // with LoRa we should send one message at a time
        while (fail_count < CLOUD_SEND_RETRY_COUNT && k_msgq_num_used_get(&transmit_queue) > 0)
        {
            /*k_event_post(&lora_request_event, LORA_LEN_REQUEST_BIT);
            LOG_INF("LEN request posted");*/

            // wait for payload len
            int max_payload_len = lora_get_max_payload_len();

            uint8_t payload[LORA_PAYLOAD_MAX_LEN]; 
            int msg_count = 0;
            int payload_pos = 0;
            bool error = false;
            // peek first and remove only after successfull transmit
            while (!error && k_msgq_peek_at(&transmit_queue, &data, msg_count) == 0)
            {
                LOG_INF("%s %u", data.id, (unsigned int)data.timestamp);
                int size = get_message_len(&data);
                if (size < 0 || payload_pos + size > max_payload_len)
                {
                    LOG_INF("Payload full: size %d, pos %d", size, payload_pos);
                    break; 
                }

                int written = serialize_payload(payload + payload_pos, &data);
                if (written < 0) {
                    LOG_ERR("Serialization error");
                    error = true;
                    break;
                }
                payload_pos += written;
                ++msg_count;
            }
            // send the payload to lora.c 
            if (!error && payload_pos > 0)
            {
                int result = lora_queue_payload(payload, payload_pos);
                if (result == 0)
                {
                    //remove only if it was sent successfuly within 30s
                    uint32_t ev = k_event_wait(&lora_response_event, LORA_MESSAGE_SENT_BIT | LORA_SEND_ERROR_BIT, true, K_SECONDS(95)); // so that iof there are 3 resends, each 30s, this wouldn't timeout
                    if (ev & LORA_MESSAGE_SENT_BIT)
                    {
                        // testing log msg
                        /*struct msg_log int_log = {
                            .type = TYPE_LOG_MSG_INT,
                            .int_msg = {
                                .id = 1,
                                .count = 2,
                                .int_vals = {payload_pos, fail_count}
                            }
                        };
                        send_log_msg(&int_log);*/
                        total_payload_len += payload_pos;
                        total_send_count += lora_get_last_send_count();
                        ++day_msg_count;

                        fail_count = 0;
                        // remove message from queue after transmit - should always succeed because we already peeked the message
                        while (msg_count > 0)
                        {
                            if (k_msgq_get(&transmit_queue, &data, K_NO_WAIT) == 0)
                            {
                                --msg_count;
                            }
                            else
                            {
                                LOG_ERR("No message after succesfull peek");
                                break;
                            }
                        }
                    } else if (ev & LORA_SEND_ERROR_BIT)
                    {
                        LOG_ERR("Error bit received, send failed");
                    } else {
                        LOG_ERR("LoRa send timeout");
                        ++fail_count;
                        LOG_INF("Send failed: %d", fail_count);
                    }
                }
                else
                {
                    ++fail_count;
                    LOG_INF("Send failed: %d", fail_count);
                }
            }
        }
         //send stats once per day
        if (day_msg_count > 0) 
        {
            time_t t = time(NULL);
            int today = (int)(t / 86400); // days since epoch
            struct tm now;
            gmtime_r(&t, &now);
            if (now.tm_hour >= 21 && today != last_day) //time can be set when to send
            {
                struct msg_log int_log = {
                    .type = TYPE_LOG_MSG_INT,
                    .int_msg = {
                        .id = 1, //id can be set here differently
                        .count = 4,
                        .int_vals = {total_payload_len / day_msg_count, total_send_count / day_msg_count, day_msg_count, bat_voltage}
                    }
                };
                send_log_msg(&int_log);
                total_payload_len = 0;
                total_send_count = 0;
                day_msg_count = 0;
                last_day = today;
            }
        }
    }
}

#endif