#pragma once

#include <zephyr/kernel.h>
#include "interfaces/itransport.hpp"
#include "interfaces/iserializer.hpp"

class CloudSender {
public:
    static constexpr int WAKEUP_PERIOD_M = 10;   // minutes between auto-wakeups
    static constexpr int SEND_RETRY_COUNT = 3;

    CloudSender(ITransport &transport, ISerializer &serializer);

    void notify();// wake up now(called from shell `transmit` cmd)
    void set_bat_voltage(int mv);

private:
    ITransport &transport_;
    ISerializer &serializer_;

    struct k_thread thread_;
    struct k_event wakeup_event_;
    struct k_timer wakeup_timer_;

    int bat_voltage_;

    //daily stat variables
    uint32_t total_payload_len_;
    uint32_t day_msg_count_;
    uint32_t total_send_count_;
    int last_day_;

    static constexpr uint32_t WAKEUP_BIT = 1;

    static void thread_entry(void *p1, void *p2, void *p3);
    static void timer_cb(struct k_timer *timer);

    void run();
    void send_log_stats();
    int  send_log_raw(const uint8_t *buf, int len);

};
