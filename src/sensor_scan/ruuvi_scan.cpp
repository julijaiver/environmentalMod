#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ruuvi_scanner);

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "sensor_scan/ruuvi_scan.hpp"
#include "sensor_topics.hpp"
#include "utils/app_events.hpp"

//ruuvi constants from old ruuvitag.h
static constexpr uint16_t MANUFACTURER_ID = 0x0499;
static constexpr uint8_t MIN_ADV_LENGTH = 7;
static constexpr uint8_t RUUVI_RAW2_LENGTH = 24;
static constexpr uint8_t FLAGS_LENGTH = 2;
static constexpr uint8_t FLAGS_TYPE = 0x01;
static constexpr uint8_t MD_TYPE = 0xff;

static constexpr int RUUVI_STACKSIZE = 4096;
static constexpr int RUUVI_THREAD_PRIORITY = 7;

K_THREAD_STACK_DEFINE(ruuvi_stack, RUUVI_STACKSIZE);

RuuviScanner *RuuviScanner::inst_ = nullptr;

void RuuviScanner::thread_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    static_cast<RuuviScanner *>(p1)->run();
}

RuuviScanner::RuuviScanner(Topic<ruuvi_data> &topic, NvStorage &nv):
                            topic_(topic), nv_(nv), pub_(topic), last_pressure_(0.0f)
{
    inst_ = this;
    k_msgq_init(&ruuvi_q_, reinterpret_cast<char *>(ruuvi_buf_), sizeof(ruuvi_data), MAX_RUUVI_TAG);
    k_event_init(&scan_event_);
    k_thread_create(&thread_, ruuvi_stack, K_THREAD_STACK_SIZEOF(ruuvi_stack), thread_entry, 
                    this, nullptr, nullptr, RUUVI_THREAD_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(&thread_, "ruuvi_scan");
}

void RuuviScanner::run()
{
    int err = bt_enable(nullptr);
    if (err) {
        for (int tries = 0; tries < 5 && err; ++tries) {
            k_msleep(5000);
            err = bt_enable(nullptr);
        }
        if (err) {
            LOG_ERR("BT init failed");
            return;
        }
    }

    AppEvents::boot_wait();
    AppEvents::clock_wait();

    nv_.read_tags();
    int all_tags = nv_.get_tag_mask();

    static struct bt_le_scan_param scan_params = {
        .type    = BT_LE_SCAN_TYPE_PASSIVE,
        .options = BT_LE_SCAN_OPT_FILTER_DUPLICATE,
        .interval = BT_GAP_SCAN_FAST_INTERVAL,
        .window   = BT_GAP_SCAN_FAST_WINDOW,
    };

    while (true) {
        AppEvents::wait(AppEvents::RUUVI_READ, false, K_FOREVER);
        AppEvents::clear(AppEvents::RUUVI_READ);

        LOG_INF("Starting bluetooth scan %02X", all_tags);
        k_event_clear(&scan_event_, all_tags);

        bt_le_scan_start(&scan_params, ble_cb);
        k_event_wait_all(&scan_event_, all_tags, false,
                         K_MSEC(AppEvents::MEASURE_CYCLE_MINUTES * 30000));
        bt_le_scan_stop();

        ruuvi_data data;
        while (k_msgq_get(&ruuvi_q_, &data, K_NO_WAIT) == 0) {
            pub_.publish(data);
        }
    }
}

float RuuviScanner::get_last_pressure() const 
{
    return last_pressure_;
}

void RuuviScanner::ble_cb(const bt_addr_le_t *addr, int8_t rssi, uint8_t type, struct net_buf_simple *ad)
{
    if (inst_) {
        inst_->on_ble_found(addr, rssi, type, ad);
    }
}

void RuuviScanner::on_ble_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type, struct net_buf_simple *ad)
{
    int tag = nv_.is_listed(&addr->a);
    if (!tag) return;

    ruuvi_data data = {};
    bt_addr_le_to_str(addr, data.mac, sizeof(data.mac));
    data.mac[strcspn(data.mac, " ")] = '\0'; // truncate at first space

    LOG_INF("Device: %02X %s", tag, data.mac);

    // skip if already seen this tag in this scan round
    if (k_event_wait(&scan_event_, tag, false, K_NO_WAIT)) return;

    if (validate(&data, addr, rssi, type, ad) < 0) return;

    data.timestamp  = time(nullptr);
    last_pressure_  = data.pressure;

    LOG_INF("t: %f, p: %f, h: %f, b: %f",
            (double)data.temperature, (double)data.pressure,
            (double)data.humidity, (double)data.bat_voltage);

    if (k_msgq_put(&ruuvi_q_, &data, K_NO_WAIT) == 0) {
        k_event_post(&scan_event_, tag);
    }
}


int16_t RuuviScanner::read_i16_be(uint8_t *d)
{
    return static_cast<int16_t>((*d << 8) | *(d + 1));
}

uint16_t RuuviScanner::read_u16_be(uint8_t *d)
{
    return static_cast<uint16_t>((*d << 8) | *(d + 1));
}

uint16_t RuuviScanner::read_u16_le(uint8_t *d)
{
    return static_cast<uint16_t>((*(d + 1) << 8) | *d);
}

int RuuviScanner::parse_ruuvitag(ruuvi_data *out, uint8_t *p)
{
    out->temperature = static_cast<float>(static_cast<int16_t>(read_u16_be(p + 1))) * 0.005f;
    out->humidity = read_u16_be(p + 3) * 0.0025f;
    out->pressure = (read_u16_be(p + 5) + 50000) / 100.0f;
    out->bat_voltage = (read_u16_be(p + 13) >> 5) / 1000.0f + 1.6f;
    return 0;
}

int RuuviScanner::validate(ruuvi_data *out, const bt_addr_le_t *addr, int8_t rssi, uint8_t type, struct net_buf_simple *ad)
{
    if (ad->len < MIN_ADV_LENGTH) return -1;

    uint8_t *buf = ad->data;

    if (*buf != FLAGS_LENGTH)  return -2;
    buf++;
    if (*buf != FLAGS_TYPE)    return -3;
    buf += 2;

    uint8_t md_length = *buf++;
    if (*buf != MD_TYPE)       return -4;
    buf++;

    if (read_u16_le(buf) != MANUFACTURER_ID) return -5;
    buf += 2;

    if ((ad->len - MIN_ADV_LENGTH) < (md_length - 3))    return -6;
    if ((ad->len - MIN_ADV_LENGTH) < RUUVI_RAW2_LENGTH) return -7;

    return parse_ruuvitag(out, buf);
}



