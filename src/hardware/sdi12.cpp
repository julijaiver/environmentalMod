#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sdi12_bus);

#include <string.h>
#include "hardware/sdi12.hpp"

#define DEV_SDI12        DEVICE_DT_GET(DT_NODELABEL(uart1))
#define ZEPHYR_USER_NODE DT_PATH(zephyr_user)

//even-parity lookup table 
char SDI12Bus::add_even_parity(char ch)
{
    static const char parity_e[] = {
        0x00, 0x81, 0x82, 0x03, 0x84, 0x05, 0x06, 0x87,
        0x88, 0x09, 0x0A, 0x8B, 0x0C, 0x8D, 0x8E, 0x0F,
        0x90, 0x11, 0x12, 0x93, 0x14, 0x95, 0x96, 0x17,
        0x18, 0x99, 0x9A, 0x1B, 0x9C, 0x1D, 0x1E, 0x9F,
        0xA0, 0x21, 0x22, 0xA3, 0x24, 0xA5, 0xA6, 0x27,
        0x28, 0xA9, 0xAA, 0x2B, 0xAC, 0x2D, 0x2E, 0xAF,
        0x30, 0xB1, 0xB2, 0x33, 0xB4, 0x35, 0x36, 0xB7,
        0xB8, 0x39, 0x3A, 0xBB, 0x3C, 0xBD, 0xBE, 0x3F,
        0xC0, 0x41, 0x42, 0xC3, 0x44, 0xC5, 0xC6, 0x47,
        0x48, 0xC9, 0xCA, 0x4B, 0xCC, 0x4D, 0x4E, 0xCF,
        0x50, 0xD1, 0xD2, 0x53, 0xD4, 0x55, 0x56, 0xD7,
        0xD8, 0x59, 0x5A, 0xDB, 0x5C, 0xDD, 0xDE, 0x5F,
        0x60, 0xE1, 0xE2, 0x63, 0xE4, 0x65, 0x66, 0xE7,
        0xE8, 0x69, 0x6A, 0xEB, 0x6C, 0xED, 0xEE, 0x6F,
        0xF0, 0x71, 0x72, 0xF3, 0x74, 0xF5, 0xF6, 0x77,
        0x78, 0xF9, 0xFA, 0x7B, 0xFC, 0x7D, 0x7E, 0xFF
    };
    return parity_e[static_cast<unsigned char>(ch) & 0x7F];
}

char SDI12Bus::remove_even_parity(char ch)
{
    return ch & 0x7F;
}

void SDI12Bus::send_break()
{
    k_msleep(50);
    gpio_pin_set_dt(&break_gpio_, 1);
    k_msleep(15);
    gpio_pin_set_dt(&break_gpio_, 0);
    k_msleep(10);
}

SDI12Bus::SDI12Bus()
    : break_gpio_(GPIO_DT_SPEC_GET(ZEPHYR_USER_NODE, break_pin_gpios))
{
}

int SDI12Bus::init()
{
    LOG_INF("SDI12Bus starting...");

    if (!gpio_is_ready_dt(&break_gpio_)) {
        LOG_ERR("Break GPIO not ready");
        return -ENODEV;
    }

    int ret = gpio_pin_configure_dt(&break_gpio_, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        LOG_ERR("Failed to configure break GPIO");
        return -1;
    }

    ret = UART::init(DEV_SDI12);
    if (ret < 0) {
        LOG_ERR("SDI12 UART init failed: %d", ret);
        return ret;
    }

    LOG_INF("SDI12 bus initialized");
    return 0;
}

int SDI12Bus::flush()
{
    clear_rcv_evt();
    char tmp[32];
    int n;
    do {
        n = read(tmp, sizeof(tmp) - 1);
    } while (n > 0);
    return 0;
}

int SDI12Bus::cmd(const char *cmd_str, bool do_break)
{
    int len = strlen(cmd_str);
    char buf[len + 1];
    for (int i = 0; i < len; ++i) {
        buf[i] = add_even_parity(cmd_str[i]);
    }
    buf[len] = '\0';

    if (do_break) {
        send_break();
    }

    flush();

    return write(reinterpret_cast<uint8_t *>(buf), len);
}

int SDI12Bus::wait_for(char *buffer, int size, const char *expect)
{
    int pos = 0;
    buffer[0] = '\0';

    if (!wait_rcv_evt(K_MSEC(3000))) {
        LOG_INF("No response: %s", buffer);
        return 0;
    }

    do {
        clear_rcv_evt();
        int remain = size - pos - 1;
        if (remain < 1) {
            return -2; // buffer full
        }

        char *buf = buffer + pos;
        int ret = read(buf, remain);
        if (ret > 0) {
            for (int i = 0; i < ret; ++i) {
                buf[i] = remove_even_parity(buf[i]);
            }
            buf[ret] = '\0';
            pos += ret;
            LOG_HEXDUMP_DBG(buf, (uint32_t)ret, "RCV:");

            if (strchr(buffer, '\n')) {
                LOG_INF("Response: %s", buffer);
                if (expect == nullptr || strstr(buffer, expect)) {
                    return static_cast<int>(strlen(buffer));
                }
            }
        }
    } while (wait_rcv_evt(K_MSEC(1000)));

    LOG_INF("No response: %s", buffer);
    return 0;
}
