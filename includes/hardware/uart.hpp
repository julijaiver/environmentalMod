//modem pipe UART handling class for sending/receiving (SDI12 bus/LoRa state machine/4G modem transport)
#pragma once

#include <zephyr/kernel.h>
#include <zephyr/modem/backend/uart.h>
#include <zephyr/modem/pipe.h>

class UART {
    protected:
        //this func will be override in Lora
        //default on_rx posts receive evt, for use for example in SDI12 bus
        virtual void on_rx() {k_event_post(&rcv_evt_, UART_RECEIVE_EVENT);}
    public:
        int init(const struct device *uart_dev);
        int write(const uint8_t *buf, int len);
        int read(char *buf, int size);
        int flush(char *buf, int size);
        bool read_line(char *out, int size, k_timeout_t timeout);
        virtual ~UART() = default;

    protected:
        bool wait_rcv_evt(k_timeout_t timeout);
        void clear_rcv_evt();

    private:
        static constexpr int RX_SIZE = 700;
        static constexpr int TX_SIZE = 550;

        static constexpr uint32_t UART_RECEIVE_EVENT = 1;

        const struct device *dev_;
        uint8_t rx_buf_[RX_SIZE];
        uint8_t tx_buf_[TX_SIZE];
        struct modem_backend_uart mdm_uart_back_;
        struct modem_pipe *mdm_uart_pipe_;
        //events are used to handle modem pipe events
        struct k_event rcv_evt_;
        static void pipe_event_handler(struct modem_pipe *pipe, enum modem_pipe_event evt, void *user_data);
    
};