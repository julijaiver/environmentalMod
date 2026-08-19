#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cloud_send);

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "d_pipeline/cloud_sender.hpp"

#include "utils/app_events.hpp"
#include "data_types.hpp"
#include "transport/lora_statemachine.hpp"

extern "C" {
#include "sensors.pb.h"
#include "pb_encode.h"
}

static constexpr int PB_BUF_MAX = LoRaStateMachine::PAYLOAD_MAX_LEN; //set initially for DR1

K_THREAD_STACK_DEFINE(cloud_send_stack_, 16384); //16KB 

CloudSender::CloudSender(ITransport &transport, ISerializer &serializer)
    : transport_(transport), serializer_(serializer), bat_voltage_(0), total_payload_len_(0)
    , day_msg_count_(0), total_send_count_(0), last_day_(-1)
{
    k_event_init(&wakeup_event_);
    k_timer_init(&wakeup_timer_, timer_cb, nullptr);
    k_timer_user_data_set(&wakeup_timer_, this);
    k_thread_create(&thread_, cloud_send_stack_,
                    K_THREAD_STACK_SIZEOF(cloud_send_stack_),
                    thread_entry, this, nullptr, nullptr,
                    7, 0, K_NO_WAIT);
    k_thread_name_set(&thread_, "cloud_send");
}

void CloudSender::notify()
{
    k_event_post(&wakeup_event_, WAKEUP_BIT);
}

void CloudSender::set_bat_voltage(int mv)
{
    bat_voltage_ = mv;
}

int CloudSender::send_log_raw(const uint8_t *buf, int len)
{
    return transport_.send_log(buf, len);
}

void CloudSender::thread_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    static_cast<CloudSender *>(p1)->run();
}

void CloudSender::timer_cb(struct k_timer *timer)
{
    static_cast<CloudSender *>(k_timer_user_data_get(timer))->notify();
}

void CloudSender::send_log_stats()
{
    LogPayload payload = LogPayload_init_zero;
    uint8_t buf[DR1_LEN];

    payload.which_content = LogPayload_int_log_tag;
    payload.content.int_log.vals[0] = day_msg_count_ > 0 ? total_payload_len_ / day_msg_count_ : 0;
    payload.content.int_log.vals[1] = day_msg_count_ > 0 ? total_send_count_  / day_msg_count_ : 0;
    payload.content.int_log.vals[2] = day_msg_count_;
    payload.content.int_log.vals[3] = (uint32_t)bat_voltage_;
    payload.content.int_log.vals_count = 4;

    pb_ostream_t stream = pb_ostream_from_buffer(buf, sizeof(buf));
    if (pb_encode(&stream, LogPayload_fields, &payload)) {
        send_log_raw(buf, (int)stream.bytes_written);
    }

    total_payload_len_ = 0;
    total_send_count_  = 0;
    day_msg_count_     = 0;
}

void CloudSender::run()
{
    AppEvents::boot_wait();

    k_timer_start(&wakeup_timer_, K_MINUTES(WAKEUP_PERIOD_M), K_MINUTES(WAKEUP_PERIOD_M));

    static uint8_t pb_buf[PB_BUF_MAX];

    while (true) {
        k_event_wait(&wakeup_event_, WAKEUP_BIT, true, K_FOREVER);
        LOG_INF("wakeup, pending: %d", serializer_.pending());

        while (serializer_.pending() > 0)
        {
            int max_len = transport_.max_payload();
            if (max_len <= 0) {
                LOG_WRN("Transport not ready, skipping send");
                break;
            }

            int len = serializer_.pack(pb_buf, max_len);
            if (len <= 0) {
                LOG_INF("Nothing packed (len=%d)", len);
                break;
            }

            bool sent = false;
            for (int attempt = 0; attempt < SEND_RETRY_COUNT && !sent; ++attempt) {
                int rc = transport_.connect();
                if (rc == 0) {
                    int result = transport_.send(pb_buf, len);
                    transport_.disconnect();

                    if (result == 0) {
                        total_payload_len_ += (uint32_t)len;
                        ++total_send_count_;
                        ++day_msg_count_;
                        LOG_INF("Send OK: %d bytes", len);
                        sent = true;
                    } else {
                        LOG_ERR("Send failed (%d), attempt %d", result, attempt + 1);
                    }
                } else {
                    LOG_ERR("connect failed (%d), attempt %d", rc, attempt + 1);
                }
            }

            if (!sent) {
                LOG_ERR("Giving up after %d attempts", SEND_RETRY_COUNT);
                break;
            }
        }

        if (day_msg_count_ > 0) {
            time_t t = time(nullptr);
            struct tm now;
            gmtime_r(&t, &now);
            int today = (int)(t / 86400);
            if (now.tm_hour >= 00 && today != last_day_) {
                send_log_stats();
                last_day_ = today;
            }
        }
    }
}
