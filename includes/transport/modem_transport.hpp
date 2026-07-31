#pragma once

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include "interfaces/itransport.hpp"
#include "transport/jwt_builder.hpp"
#include "hardware/uart.hpp"

// connected modem.c, cloud.c, device.c GPIO/power, and errors.c retry logic.
//only used when CONFIG_CLOUD_SEND_4G=y.
class ModemTransport : public ITransport {
    struct gpio_dt_spec modem_gpio_;
    JwtBuilder jwt_;
    char *access_token_;
    UART uart_;

public:
    ModemTransport();

    //config modem gpio in main
    int  init();

    //Power on modem, sync system clock, power off
    //handles retries internally. Returns 0 on success
    int  boot_setup();

    int  connect() override;
    int  send(const uint8_t *buf, int len) override;
    int disconnect() override;
    int  send_log(const uint8_t *buf, int len) override;
    int  max_payload() override;

private:
    static constexpr int AT_RESPONSE_SIZE = 1024;
    static constexpr int BOOT_AT_TIMEOUT_MS = 5000;
    static constexpr int POWER_ON_DELAY_MS = 15000;
    static constexpr int STARTUP_DELAY_MS = 20000;
    static constexpr int NET_SETTLE_DELAY_MS = 12000;
    static constexpr int MAX_INIT_TRIES = 5;
    static constexpr int RETRY_DELAY_MS = 10000;

    void power_on();
    void power_off();

    bool at(const char *cmd, const char *expect, k_timeout_t timeout);
    bool at_len(const char *cmd, size_t len, const char *expect, k_timeout_t timeout);

    int  modem_init();
    int  modem_get_time(char *buf, size_t size);
    int  fetch_access_token();

    int  http_start();
    void http_stop();
    int  http_post(const char *url, const char *content_type, const char *body, size_t body_len);
    int  http_read_access_token(char *out, size_t out_size);

    int  tcp_open();
    void tcp_close();
    void tcp_send_chunked(const char *buf, size_t len);

    static char *delete_char(char *s, char ch);
};
