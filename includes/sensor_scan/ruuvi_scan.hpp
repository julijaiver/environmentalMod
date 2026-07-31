#pragma once

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>

struct net_buf_simple;
#include "pubsub/publisher.hpp"
#include "data_types.hpp"
#include "storage/nv_storage.hpp"

class RuuviScanner {
    public:
        explicit RuuviScanner(Topic<ruuvi_data> &topic, NvStorage &nv);
        float get_last_pressure() const;
    private:
        static constexpr int MAX_RUUVI_TAG = 10;

        Topic<ruuvi_data> &topic_;
        NvStorage &nv_;
        Publisher<ruuvi_data> pub_;

        alignas(ruuvi_data) uint8_t ruuvi_buf_[sizeof(ruuvi_data) * MAX_RUUVI_TAG];
        static RuuviScanner *inst_;
        struct k_msgq ruuvi_q_;
        struct k_thread thread_;
        struct k_event scan_event_;
        float last_pressure_;
        
        static void thread_entry(void *p1, void *p2, void *p3);
        static void ble_cb(const bt_addr_le_t *addr, int8_t rssi, uint8_t type, struct net_buf_simple *ad);
        void on_ble_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type, struct net_buf_simple *ad);
        static int16_t read_i16_be(uint8_t *data);
        static uint16_t read_u16_be(uint8_t *data);
        static uint16_t read_u16_le(uint8_t *data);
        static int parse_ruuvitag(ruuvi_data *out, uint8_t *data_ptr);
        static int validate(ruuvi_data *out, const bt_addr_le_t *addr, int8_t rssi, uint8_t type, struct net_buf_simple *ad);

        void run();

};