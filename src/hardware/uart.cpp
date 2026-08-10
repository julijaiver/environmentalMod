#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(uart_cpp);

#include "hardware/uart.hpp"
#include <string.h>

#define UART_RCV_EVT 1

void UART::pipe_event_handler(struct modem_pipe *pipe, enum modem_pipe_event evt, void *user_data)
{
    UART *self = static_cast<UART *>(user_data);
    switch(evt) {
        case MODEM_PIPE_EVENT_RECEIVE_READY:
            LOG_DBG("receive ready");
            self->on_rx();
            break;
        case MODEM_PIPE_EVENT_TRANSMIT_IDLE:
            LOG_DBG("transmit idle");
            break;
        default:
            LOG_DBG("modem evt: %d", evt);
            break;
    }
}

//GET DT LABEL for UARt will be used in LoraStMachine.cpp and passed here
int UART::init(const struct device *uart_dev)
{
    dev_ = uart_dev;

    if(!device_is_ready(dev_)) {
        LOG_ERR("uart device not ready");
        return -ENODEV;
    }
    k_event_init(&rcv_evt_);

    //modem pipe config
    const struct modem_backend_uart_config cfg = {
        .uart            = dev_,
        .receive_buf     = rx_buf_,
        .receive_buf_size = sizeof(rx_buf_),
        .transmit_buf    = tx_buf_,
        .transmit_buf_size = sizeof(tx_buf_),
    };
        mdm_uart_pipe_ = modem_backend_uart_init(&mdm_uart_back_, &cfg);
    if (!mdm_uart_pipe_) {
        LOG_ERR("Failed to init modem backend");
        return -EIO;
    }

    modem_pipe_attach(mdm_uart_pipe_, pipe_event_handler, this);

    int ret = modem_pipe_open(mdm_uart_pipe_, K_MSEC(100));
    if (ret < 0) {
        LOG_ERR("Failed to open modem pipe: %d", ret);
        return ret;
    }

    LOG_INF("Uart modem pipe initialized");
    return 0;
}

int UART::write(const uint8_t *buf, int len)
{
    return modem_pipe_transmit(mdm_uart_pipe_, buf, len);
}

int UART::read(char *buf, int size)
{
    int ret = modem_pipe_receive(mdm_uart_pipe_, reinterpret_cast<uint8_t *>(buf), size);
    if (ret > 0) {
        buf[ret] = '\0';
        LOG_HEXDUMP_DBG(buf, (uint32_t)ret, "RCV:");
    }
    return ret;
}

int UART::flush(char *buf, int size)
{
    int count = 0;
    int rv;
    do {
        rv = modem_pipe_receive(mdm_uart_pipe_, reinterpret_cast<uint8_t *>(buf), size);
        if (rv > 0) {
            LOG_INF("flushed: %d", rv);
            count += rv;
        }
    } while (rv > 0);
    return count;
}

bool UART::read_line(char *out, int size, k_timeout_t timeout)
{
    int pos = 0;
    while (pos < size - 1) {
        if (!wait_rcv_evt(timeout))
            return false;
        clear_rcv_evt();
        int n = read(out + pos, size - pos - 1);
        if (n > 0) {
            pos += n;
            out[pos] = '\0';
            char *nl = static_cast<char *>(memchr(out, '\n', pos));
            if (nl) {
                *nl = '\0';
                if (nl > out && *(nl - 1) == '\r')
                    *(nl - 1) = '\0';
                return true;
            }
        }
    }
    return false;
}

bool UART::wait_rcv_evt(k_timeout_t timeout)
{
    return k_event_wait(&rcv_evt_, UART_RECEIVE_EVENT, false, timeout) != 0;
}

void UART::clear_rcv_evt()
{
    k_event_clear(&rcv_evt_, UART_RECEIVE_EVENT);
}


