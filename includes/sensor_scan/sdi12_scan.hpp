#pragma once

#include <zephyr/kernel.h>
#include "pubsub/publisher.hpp"
#include "hardware/sdi12.hpp"
#include "sensor_scan/ruuvi_scan.hpp"
#include "data_types.hpp"

class SDI12Scanner {
    public:
        SDI12Scanner(SDI12Bus &bus, RuuviScanner &ruuvi, 
                    Topic<teros12_data> &teros_topic, Topic<solyx14_data> &solyx_topic, Topic<solinst_data> &solinst_topic);
    
    private:
        static constexpr int MAX_SENSORS = 3;

        Publisher<teros12_data> pub_teros_;
        Publisher<solyx14_data> pub_solyx_;
        Publisher<solinst_data> pub_solinst_;

        SDI12Bus &bus_;
        RuuviScanner &ruuvi_;
        struct k_thread thread_;
        struct sensor_info {
            char addr;
            int type;
            char id[32];
        };

        static void thread_entry(void *p1, void *p2, void *p3);
        int scan_sensors(sensor_info *sensors, int max);
        int read_teros12(const sensor_info &s);
        int read_solyx14(const sensor_info &s);
        int read_solinst(const sensor_info &s);

        void run();
};