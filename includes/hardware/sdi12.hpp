#pragma once

#include <zephyr/drivers/gpio.h>
#include "uart.hpp"

// pipe management from UART class and GPIO break signal

class SDI12Bus : public UART {
    struct gpio_dt_spec break_gpio_;
    struct gpio_dt_spec pwr_ctrl_gpio_;

    static char add_even_parity(char ch);
    static char remove_even_parity(char ch);
    void send_break();

public:
    SDI12Bus();

    int init();
    int cmd(const char *cmd, bool send_break);
    int wait_for(char *buf, int size, const char *expect);
    int flush();

    //for controlling sensor power
    void power_on();
    void power_off();
};
