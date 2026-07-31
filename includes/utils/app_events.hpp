#pragma once

#include <zephyr/kernel.h>

//functions for app events used throughout files. namespace cause only represents a module with event funcs, not really an object 
namespace AppEvents {

    static constexpr uint32_t BOOT_HALT = BIT(0);
    static constexpr uint32_t BOOT_CONTINUE = BIT(1);
    static constexpr uint32_t CLOCK_SET = BIT(2);
    static constexpr uint32_t SDI12_READ = BIT(3);
    static constexpr uint32_t RUUVI_READ = BIT(4);

    static constexpr int MEASURE_CYCLE_MINUTES = 10;

    void boot_halt();
    void boot_continue();
    void boot_wait();
    void clock_wait();
    void clock_synced();
    void take_sample_now();

    uint32_t wait(uint32_t events, bool reset, k_timeout_t timeout);
    void post(uint32_t events);
    void clear(uint32_t events);

} 
