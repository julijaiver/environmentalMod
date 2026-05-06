#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ruuvi);

#include <stdio.h>
#include <time.h>

#include "device.h"
#include "data_types.h"
#include "data_queue.h"
#include "nv_params.h"
#include "ruuvitag.h"

struct k_event ruuvi_tag_scan_event;

static struct bt_le_scan_param scan_params = {
    .type = BT_LE_SCAN_TYPE_PASSIVE,
    .interval = BT_GAP_SCAN_FAST_INTERVAL,
    .window = BT_GAP_SCAN_FAST_WINDOW,
    .options = BT_LE_SCAN_OPT_FILTER_DUPLICATE,
};

K_MSGQ_DEFINE(ruuvi_scan_queue, sizeof(struct sensor_data), MAX_RUUVI_TAG, 1);

static int ruuvi_validate(struct ruuvi_tag *ruuvi, const bt_addr_le_t *addr, int8_t rssi, uint8_t type, struct net_buf_simple *ad);
static int parse_ruuvitag(struct ruuvi_tag *ruuvi, uint8_t *data_ptr);

static void ruuvi_scan_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type, struct net_buf_simple *ad)
{
    int tag = is_listed(&addr->a);

    if (tag)
    {
        struct sensor_data data = { .type = TYPE_RUUVI_TAG };
        bt_addr_le_to_str(addr, data.id, sizeof(data.id));
        LOG_INF("Device: %02X %s", tag, data.id);
        data.id[strcspn(data.id, " ")] = 0; // truncate to first space
        if (!k_event_wait(&ruuvi_tag_scan_event, tag, false, K_NO_WAIT))
        {
            if (ruuvi_validate(&data.ruuvi, addr, rssi, type, ad) >= 0)
            {
                data.timestamp = time(NULL);
                // LOG_HEXDUMP_INF(&addr->a, sizeof(addr->a), "MAC: ");
                // LOG_HEXDUMP_INF(ad->data, ad->len, "Data: ");
                LOG_INF("t: %f, p: %f, h: %f, b: %f", (double)data.ruuvi.temperature,
                        (double)data.ruuvi.pressure, (double)data.ruuvi.humidity, (double)data.ruuvi.bat_voltage);
                // we need to use intermediate queue because we are in callback. 
                // Sending to cloud transmit queue may block to wait for space in the queue
                if (k_msgq_put(&ruuvi_scan_queue, &data, K_NO_WAIT) == 0)
                {
                    k_event_post(&ruuvi_tag_scan_event, tag);
                }
            }
        }
    }
}

#define MAX_TRIES 5
#define RETRY_DELAY_MS 5000

void ruuvi_scan_thread(void *arg0, void *arg1, void *arg2)
{
    int all_tags = 0;
    int seen_tags = 0;

    /* Initialize the Bluetooth Subsystem */
    int err = bt_enable(NULL);
    if (err)
    {
        // not sure if trying again is worth it - kept this since it was in the original code
        LOG_DBG("Trying to initialize Bluetooth again...\n");
        int tries = 0;
        while (tries < MAX_TRIES && err)
        {
            k_msleep(RETRY_DELAY_MS);
            err = bt_enable(NULL);
            tries++;
        }
        if (err)
        {
            LOG_ERR("BT init failed");
            return; // end thread if BT doesn't work
        }
    }

    k_event_init(&ruuvi_tag_scan_event);
    nvs_read_tags();
    all_tags = get_tag_mask();

    while (true)
    {
        struct sensor_data ruuvi;
        LOG_INF("Starting bluetooth scan %02X", all_tags);
        // clear event bits before scan starts
        k_event_clear(&ruuvi_tag_scan_event, all_tags);
        int err = bt_le_scan_start(&scan_params, ruuvi_scan_found);
        seen_tags = k_event_wait_all(&ruuvi_tag_scan_event, all_tags, false, K_MSEC(MEASURE_CYCLE_SLEEP));
        LOG_INF("Found sensors: %02X", seen_tags);
        err = bt_le_scan_stop();
        while (k_msgq_get(&ruuvi_scan_queue, &ruuvi, K_NO_WAIT) == 0) {
            if(data_put(&ruuvi) != 0) {
                LOG_ERR("Transmit queue error");
            }
        }

        k_msleep(MEASURE_CYCLE_SLEEP);
    }
}

static int16_t read_i16_be(uint8_t *data)
{
    return (*data << 8) | *(data + 1);
}

static uint16_t read_u16_be(uint8_t *data)
{
    return (*data << 8) | *(data + 1);
}

static uint16_t read_u16_le(uint8_t *data)
{
    return (*(data + 1) << 8) | *data;
}

static int parse_ruuvitag(struct ruuvi_tag *ruuvi, uint8_t *data_ptr)
{
    // uint8_t device_id[7]; // Device MAC-address
    float temperature; // Temperature
    float humidity;    // Humidity
    float pressure;    // Pressure
    float accel_x;     // Accel x
    float accel_y;     // Accel y
    float accel_z;     // Accel z
    float voltage;     // Voltage

    int8_t power;      // TX power
    uint8_t counter;   // Movement counter
    uint16_t sequence; // Sequence number

    temperature = (float)((int16_t)read_u16_be(data_ptr + 1)) * 0.005f;

    humidity = read_u16_be(data_ptr + 3) * 0.0025f;

    pressure = (read_u16_be(data_ptr + 5) + 50000) / 100.0f;

    accel_x = read_i16_be(data_ptr + 7) / 100.0f;

    accel_y = read_i16_be(data_ptr + 9) / 100.0f;

    accel_z = read_i16_be(data_ptr + 11) / 100.0f;

    voltage = (read_u16_be(data_ptr + 13) >> 5) / 1000.0f + 1.6f;

    power = (read_u16_be(data_ptr + 13) & 0x1f) * 2 - 40;

    counter = *(data_ptr + 15);

    sequence = read_u16_be(data_ptr + 16);

    ruuvi->humidity = humidity;
    ruuvi->pressure = pressure;
    ruuvi->temperature = temperature;
    ruuvi->bat_voltage = voltage;
    // MAC address is in following six bytes. (18-23)

    return 0;
}

static int ruuvi_validate(struct ruuvi_tag *ruuvi, const bt_addr_le_t *addr, int8_t rssi, uint8_t type, struct net_buf_simple *ad)
{
    if (ad->len < MIN_ADV_LENGTH)
    {
        LOG_INF("AD: message too short");
        return -1;
    }

    uint8_t *buffer = ad->data;

    if (*buffer != FLAGS_LENGTH)
    {
        LOG_INF("AD: no Flags structure");
        return -2;
    }

    buffer++;
    if (*buffer != FLAGS_TYPE)
    {
        LOG_INF("AD: no Flags type");
        return -3;
    }
    buffer += 2;

    uint8_t md_length = *buffer;
    buffer++;
    if (*buffer != MD_TYPE)
    {
        LOG_INF("AD: no manufacturer data type");
        return -4;
    }

    buffer++;
    uint16_t company_identifier = read_u16_le(buffer); //(*buffer_idx << 8) + *(buffer_idx + 1);
    if (company_identifier != MANUFACTURER_ID)
    {
        LOG_INF("AD: no Ruuvi identifier\n");
        return -5;
    }

    if ((ad->len - MIN_ADV_LENGTH) < (md_length - 3))
    {
        LOG_INF("Ruuvi: data too short - %d - %d", md_length - 3, ad->len - MIN_ADV_LENGTH);
        return -6;
    }
    if ((ad->len - MIN_ADV_LENGTH) < RUUVI_RAWV2_LENGTH)
    {
        LOG_INF("Ruuvi: RAWv2 data too short");
        return -7;
    }

    buffer += 2;

    return parse_ruuvitag(ruuvi, buffer);
}
