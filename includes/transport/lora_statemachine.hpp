#pragma once

#include <zephyr/kernel.h>
#include "hardware/uart.hpp"
#include <initializer_list> 

class LoRaStateMachine : public UART {
public:
    static constexpr int PAYLOAD_MAX_LEN = 51; //DR1 max payload

    // lora response bits
    static constexpr uint32_t LORA_LEN_READY_BIT = BIT(0);
    static constexpr uint32_t LORA_MESSAGE_SENT_BIT = BIT(1);
    static constexpr uint32_t LORA_SEND_ERROR_BIT = BIT(2);

    // ports for data and for log messages
    static constexpr int DATA_MSG_PORT = 2;
    static constexpr int LOG_MSG_PORT = 3;

    // public because sizeof() is used
    enum class EvType : int { Enter, Exit, Tick, Receive, Command, PayloadReady };
    struct Ev { EvType ev; int length; void *data; };
    struct Payload { uint8_t buf[PAYLOAD_MAX_LEN]; int len; int port; };

    LoRaStateMachine();

    // cloud sender called funcs
    int queue_payload(const uint8_t *buf, int len, int port);
    int wait_for_response(uint32_t bits, k_timeout_t timeout);
    int get_max_payload_len();
    int get_last_send_count();

    // shell funcs
    int raw_write(const char *str);
    int raw_read(char *str, int max_len);

private:
    struct Smi;
    using SmFn = void (LoRaStateMachine::*)(Smi *, const Ev *);

    struct k_thread thread_;
    struct k_timer tick_timer_;
    struct k_msgq event_queue_;
    struct k_msgq payload_queue_;
    int max_payload_len_;
    int last_send_count_;

    alignas(Ev) uint8_t event_buf_[20 * sizeof(Ev)];
    alignas(Payload) uint8_t payload_buf_[4 * sizeof(Payload)];

    static void thread_entry(void *p1, void *p2, void *p3);
    static void tick_cb(struct k_timer *timer);
    void on_rx() override;

    int transmit(const char *str);

    void run();
    //general lora functions
    void sm_clear(Smi *sm);
    int lora_flush(Smi *sm);
    int lora_read(Smi *sm);
    int lora_write(Smi *sm, const char *str);
    //no ** and nullptr needed
    int lora_wait_for(Smi *sm, std::initializer_list<const char*> expect);
    int send_hex(Smi *sm, Payload *payload);
    static int set_system_time(const char *str);

    //Lora states 
    void st_init(Smi *sm, const Ev *e);
    void st_mode(Smi *sm, const Ev *e);
    void st_class(Smi *sm, const Ev *e);
    void st_dr(Smi *sm, const Ev *e);
    void st_port(Smi *sm, const Ev *e);
    void st_join(Smi *sm, const Ev *e);
    void st_clock_sync(Smi *sm, const Ev *e);
    void st_connected(Smi *sm, const Ev *e);
    void st_get_payload_len(Smi *sm, const Ev *e);
    void st_send(Smi *sm, const Ev *e);
};
