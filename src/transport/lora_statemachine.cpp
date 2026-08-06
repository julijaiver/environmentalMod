#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(lora);

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "transport/lora_statemachine.hpp"

#include "utils/app_events.hpp"

#define DEV_LORA DEVICE_DT_GET(DT_NODELABEL(uart0))
#define DATA_RATE "1"
#define LORA_SM_PRIORITY 7

K_THREAD_STACK_DEFINE(lora_stack_, 8192);

K_EVENT_DEFINE(lora_response_event);

struct LoRaStateMachine::Smi {
    SmFn state;
    SmFn next_state;
    int timer;
    int count;
    int pos;
    char buffer[700];
};

#define TRAN(st) { sm->next_state = &LoRaStateMachine::st; }

// events used throughout the sm
static const LoRaStateMachine::Ev evEnter = { LoRaStateMachine::EvType::Enter, 0, nullptr };
static const LoRaStateMachine::Ev evExit = { LoRaStateMachine::EvType::Exit, 0, nullptr };
static const LoRaStateMachine::Ev evTick = { LoRaStateMachine::EvType::Tick, 0, nullptr };
static const LoRaStateMachine::Ev evReceive = { LoRaStateMachine::EvType::Receive, 0, nullptr };
static const LoRaStateMachine::Ev evCommand = { LoRaStateMachine::EvType::Command, 0, nullptr };
static const LoRaStateMachine::Ev evPayloadReady = { LoRaStateMachine::EvType::PayloadReady, 0, nullptr };


LoRaStateMachine::LoRaStateMachine()
    : max_payload_len_(50), last_send_count_(0)
{
    k_msgq_init(&event_queue_, reinterpret_cast<char *>(event_buf_), sizeof(Ev), 10);
    k_msgq_init(&payload_queue_, reinterpret_cast<char *>(payload_buf_), sizeof(Payload), 4);
    k_timer_init(&tick_timer_, tick_cb, nullptr);
    k_timer_user_data_set(&tick_timer_, this);
    k_thread_create(&thread_, lora_stack_, K_THREAD_STACK_SIZEOF(lora_stack_), 
                    thread_entry, this, nullptr, nullptr, LORA_SM_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(&thread_, "lora_sm");
}

void LoRaStateMachine::thread_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    static_cast<LoRaStateMachine *>(p1)->run();
}

void LoRaStateMachine::tick_cb(struct k_timer *timer)
{
    auto *self = static_cast<LoRaStateMachine *>(k_timer_user_data_get(timer));
    k_msgq_put(&self->event_queue_, &evTick, K_NO_WAIT);
}

void LoRaStateMachine::on_rx()
{
    k_msgq_put(&event_queue_, &evReceive, K_NO_WAIT);
}

int LoRaStateMachine::transmit(const char *str)
{
    //currently lora module should be set to AUTOON via shell and then all transmissions have wakeup bytes
    static const uint8_t autoon_prefix[] = { 0xFF, 0xFF, 0xFF, 0xFF };
    write(autoon_prefix, sizeof(autoon_prefix));
    return write(reinterpret_cast<const uint8_t *>(str), strlen(str));
}


void LoRaStateMachine::sm_clear(Smi *sm)
{
    sm->timer = 0;
    sm->count = 0;
    sm->pos = 0;
    sm->buffer[0] = '\0';
}

int LoRaStateMachine::lora_flush(Smi *sm)
{
    int count = flush(sm->buffer, sizeof(sm->buffer));
    sm->buffer[0] = '\0';
    sm->pos = 0;
    return count;
}

int LoRaStateMachine::lora_read(Smi *sm)
{
    int remain = sizeof(sm->buffer) - sm->pos - 1;
    if (remain < 1) return -2;
    int ret = read(sm->buffer + sm->pos, remain);
    if (ret > 0) {
        sm->buffer[sm->pos + ret] = '\0';
        sm->pos += ret;
    }
    return ret;
}

int LoRaStateMachine::lora_write(Smi *sm, const char *str)
{
    (void)sm;
    return transmit(str);
}

int LoRaStateMachine::lora_wait_for(Smi *sm, std::initializer_list<const char*> expect)
{
    int ret = -1;

    int len = strlen(sm->buffer);                                                    
    if (len < sm->pos) {                                                             
        memmove(sm->buffer, sm->buffer + len + 1, sm->pos - len);                    
        sm->pos -= len + 1;                                                          
        LOG_INF("Pending: %s", sm->buffer);                                        
    }   

    lora_read(sm);

    int dist = strcspn(sm->buffer, "\n");
    if (dist < sm->pos) {
        sm->buffer[dist] = '\0';
        LOG_INF("Response: [%d] %s", sm->pos - dist - 1, sm->buffer);

        int i = 0;
        for (const char *e: expect) {
            if (strstr(sm->buffer, e)) {
                ret = i;
                break;
            }
            ++i;
        }
        if (ret < 0) ret = -2;

        if (sm->pos - dist <= 1) {
            sm->pos = 0;
        } else {
            // re-post receive event so pending data gets processed
            k_msgq_put(&event_queue_, &evReceive, K_NO_WAIT);
        }
    }

    LOG_INF("Ridx: %d", ret);
    return ret;
}

int LoRaStateMachine::send_hex(Smi *sm, Payload *payload)
{
    if (payload->len <= 0) return -1;

    static char hex_str[2 * PAYLOAD_MAX_LEN + 1];
    static char cmd_buf[2 * PAYLOAD_MAX_LEN + 16];

    for (int i = 0; i < payload->len; ++i) {
        snprintf(hex_str + 2 * i, sizeof(hex_str) - 2 * i, "%02X", payload->buf[i]);
    }
    LOG_INF("Payload: %d bytes", payload->len);
    snprintf(cmd_buf, sizeof(cmd_buf), "AT+CMSGHEX=\"%s\"\r\n", hex_str);
    return lora_write(sm, cmd_buf);
}


void LoRaStateMachine::run()
{
    const struct device *uart_dev = DEV_LORA;

    if (!device_is_ready(uart_dev)) {
        LOG_ERR("LoRa UART not ready");
        return;
    }

    if (init(uart_dev) < 0) {
        LOG_ERR("LoRa UART init failed");
        return;
    }

    AppEvents::boot_wait();

    Smi sm = {};
    sm.state = &LoRaStateMachine::st_init;
    sm.next_state = &LoRaStateMachine::st_init;

    (this->*sm.state)(&sm, &evEnter);

    k_timer_start(&tick_timer_, K_MSEC(1000), K_MSEC(1000));

    Ev ev;
    while (true) {
        if (k_msgq_get(&event_queue_, &ev, K_FOREVER) == 0) {
            (this->*sm.state)(&sm, &ev);
            if (sm.state != sm.next_state) {
                (this->*sm.state)(&sm, &evExit);
                sm.state = sm.next_state;
                (this->*sm.state)(&sm, &evEnter);
            }
        } else {
            LOG_ERR("No message");
        }
    }
}


int LoRaStateMachine::queue_payload(const uint8_t *buf, int len, int port)
{
    if (len > PAYLOAD_MAX_LEN) {
        LOG_ERR("Payload too long: %d", len);
        return -1;
    }
    Payload p;
    memcpy(p.buf, buf, len);
    p.len  = len;
    p.port = port;
    int rc = k_msgq_put(&payload_queue_, &p, K_NO_WAIT);
    if (rc == 0) {
        k_msgq_put(&event_queue_, &evPayloadReady, K_NO_WAIT);
    }
    return rc;
}

int LoRaStateMachine::wait_for_response(uint32_t bits, k_timeout_t timeout)
{
    uint32_t ev = k_event_wait(&lora_response_event, bits, true, timeout);
    if (ev & LORA_MESSAGE_SENT_BIT) {
        return 0;
    }
    if (ev & LORA_SEND_ERROR_BIT) {
        return -EIO;
    }   
    return -ETIMEDOUT;
}

int LoRaStateMachine::get_max_payload_len()
{
    k_event_clear(&lora_response_event, LORA_LEN_READY_BIT);
    k_msgq_put(&event_queue_, &evCommand, K_NO_WAIT);
    if (k_event_wait(&lora_response_event, LORA_LEN_READY_BIT, false, K_SECONDS(10)) == 0) {
        return -1; // timeout
    }
    return max_payload_len_;
}

int LoRaStateMachine::get_last_send_count()
{
    return last_send_count_;
}

int LoRaStateMachine::raw_write(const char *str)
{
    return transmit(str);
}

int LoRaStateMachine::raw_read(char *str, int max_len)
{
    int pos = 0;
    int ret;
    str[0] = '\0';
    Ev ev;
    do {
        k_msgq_get(&event_queue_, &ev, K_MSEC(1000));
        int remain = max_len - pos - 1;
        if (remain < 1) return -2;
        ret = read(str + pos, remain);
        if (ret > 0) {
            pos += ret;
        }
    } while (ret > 0);
    return pos;
}

int LoRaStateMachine::set_system_time(const char *str)
{
    if (!str) {
        return -1;
    }

    int year = 0, month = 0, day = 0, hour = 0, min = 0, sec = 0, msec = 0;
    // Format: " 2026-04-30 12:30:49.465"
    int parsed = sscanf(str, "%d-%d-%d %d:%d:%d.%d",
                        &year, &month, &day, &hour, &min, &sec, &msec);
    if (parsed != 7) {
        LOG_ERR("Invalid time format: %d", parsed);
        return -2;
    }

    struct tm now = {};
    now.tm_year = year - 1900;
    now.tm_mon = month - 1;
    now.tm_mday = day;
    now.tm_hour = hour;
    now.tm_min = min;
    now.tm_sec = sec;
    now.tm_isdst = 0;

    time_t epoch = mktime(&now);
    if (epoch == (time_t)-1) {
        LOG_ERR("Invalid time struct");
        return -3;
    }

    struct timespec ts = {
        .tv_sec  = epoch,
        .tv_nsec = (long)msec * 1000000L,
    };

    if (clock_settime(CLOCK_REALTIME, &ts) != 0) {
        LOG_ERR("Failed to set system time");
        return -4;
    }

    LOG_INF("System time set");
    return 0;
}

void LoRaStateMachine::st_init(Smi *sm, const Ev *e)
{
    switch (e->ev) {
    case EvType::Enter:
        lora_flush(sm);
        sm_clear(sm);
        break;
    case EvType::Tick:
        lora_write(sm, "AT\r\n");
        break;
    case EvType::Receive:
        if (lora_wait_for(sm, { "+AT: OK" }) == 0) { 
            TRAN(st_mode);
        }
        break;
    default:
        break;
    }
}

void LoRaStateMachine::st_mode(Smi *sm, const Ev *e)
{
    switch (e->ev) {
    case EvType::Enter:
        lora_flush(sm);
        sm_clear(sm);
        lora_write(sm, "AT+MODE=LWOTAA\r\n");
        break;
    case EvType::Tick:
        if (++sm->timer > 3) { 
            TRAN(st_init); 
        }
        break;
    case EvType::Receive:
        if (lora_wait_for(sm, { "+MODE: LWOTAA" }) == 0) { 
            TRAN(st_class); 
        }
        break;
    default:
        break;
    }
}

void LoRaStateMachine::st_class(Smi *sm, const Ev *e)
{
    switch (e->ev) {
    case EvType::Enter:
        lora_flush(sm);
        sm_clear(sm);
        lora_write(sm, "AT+CLASS=A\r\n");
        break;
    case EvType::Tick:
        if (++sm->timer > 3) { TRAN(st_init); }
        break;
    case EvType::Receive:
        if (lora_wait_for(sm, { "+CLASS: A" }) == 0) { 
            TRAN(st_dr); 
        }
        break;
    default:
        break;
    }
}

void LoRaStateMachine::st_dr(Smi *sm, const Ev *e)
{
    switch (e->ev) {
    case EvType::Enter:
        lora_flush(sm);
        sm_clear(sm);
        lora_write(sm, "AT+DR=" DATA_RATE "\r\n");
        break;
    case EvType::Tick:
        if (++sm->timer > 3) { TRAN(st_init); }
        break;
    case EvType::Receive:
        if (lora_wait_for(sm, { "+DR: DR" DATA_RATE}) == 0) { 
            TRAN(st_join); 
        }
        break;
    default:
        break;
    }
}

void LoRaStateMachine::st_port(Smi *sm, const Ev *e)
{
    switch (e->ev) {
    case EvType::Enter: {
        lora_flush(sm);
        sm_clear(sm);
        Payload payload;
        if (k_msgq_peek(&payload_queue_, &payload) == 0) {
            char cmd[16];
            snprintf(cmd, sizeof(cmd), "AT+PORT=%d\r\n", payload.port);
            lora_write(sm, cmd);
        }
        break;
    }
    case EvType::Tick:
        if (++sm->timer > 3) { TRAN(st_init); }
        break;
    case EvType::Receive:
        if (lora_wait_for(sm, { "+PORT: "}) == 0) { 
            TRAN(st_send); 
        }
        break;
    default:
        break;
    }
}

void LoRaStateMachine::st_join(Smi *sm, const Ev *e)
{

    switch (e->ev) {
    case EvType::Enter:
        lora_flush(sm);
        sm_clear(sm);
        lora_write(sm, "AT+JOIN\r\n");
        break;
    case EvType::Tick:
        if (++sm->timer == 30) {
            lora_write(sm, "AT+JOIN\r\n");
            sm->timer = 0;
        }
        break;
    case EvType::Receive: {
        int res = lora_wait_for(sm, {
        "+JOIN: Join failed",
        "+JOIN: LoRaWAN modem is busy",
        "+JOIN: Network joined",
        "+JOIN: Joined already",
    });
        if (res == 2 || res == 3) { 
            TRAN(st_clock_sync); 
        }
        break;
    }
    default:
        break;
    }
}

void LoRaStateMachine::st_clock_sync(Smi *sm, const Ev *e)
{
    switch (e->ev) {
    case EvType::Enter:
        lora_flush(sm);
        sm_clear(sm);
        lora_write(sm, "AT+LW=DTR\r\n");
        break;
    case EvType::Tick:
        if (++sm->timer > 30) {
            lora_write(sm, "AT+LW=DTR\r\n");
            sm_clear(sm);
        }
        break;
    case EvType::Receive: {
        int res = lora_wait_for(sm, { "+LW: DTR", "+CMSGHEX: Done", "+RTC:" });
        if (res == 0) {
            lora_write(sm, "AT+CMSGHEX\r\n");
        } else if (res == 1) {
            lora_write(sm, "AT+RTC=UTCMS\r\n");
        } else if (res == 2) {
            const char *tstr = strstr(sm->buffer, "+RTC:") + 5;
            int year = 0;
            if (sscanf(tstr, "%d", &year) == 1 && year >= 2026) {
                if (set_system_time(tstr) == 0) {
                    TRAN(st_connected);
                }
                AppEvents::clock_synced();
            }
        }
        break;
    }
    default:
        break;
    }
}

void LoRaStateMachine::st_connected(Smi *sm, const Ev *e)
{
    switch (e->ev) {
    case EvType::Enter:
        lora_flush(sm);
        sm_clear(sm);
        LOG_INF("LoRa connected");
        break;
    case EvType::Command:
        TRAN(st_get_payload_len);
        break;
    case EvType::PayloadReady:
        TRAN(st_port);
        break;
    default:
        break;
    }
}

void LoRaStateMachine::st_get_payload_len(Smi *sm, const Ev *e)
{
    switch (e->ev) {
    case EvType::Enter:
        lora_flush(sm);
        sm_clear(sm);
        lora_write(sm, "AT+LW=LEN\r\n");
        break;
    case EvType::Tick:
        if (++sm->timer > 10) { TRAN(st_connected); }
        break;
    case EvType::Receive: {
        int res = lora_wait_for(sm, { "+LW: LEN" });
        if (res == 0) {
            const char *len_str = strstr(sm->buffer, "+LW: LEN,");
            if (len_str) {
                sscanf(len_str + 9, "%d", &max_payload_len_);
                LOG_INF("Max payload: %d", max_payload_len_);
            }
            k_event_post(&lora_response_event, LORA_LEN_READY_BIT);
            TRAN(st_connected);
        }
        break;
    }
    default:
        break;
    }
}

void LoRaStateMachine::st_send(Smi *sm, const Ev *e)
{

    switch (e->ev) {
    case EvType::Enter: {
        lora_flush(sm);
        sm_clear(sm);
        Payload payload;
        if (k_msgq_peek(&payload_queue_, &payload) == 0) {
            if (send_hex(sm, &payload) > 0) {
                sm->timer = 0;
                sm->count = 1;
            }
        }
        break;
    }
    case EvType::Tick: {
        if (sm->count == 0) {
            if (++sm->timer > 10) {
                k_event_post(&lora_response_event, LORA_SEND_ERROR_BIT);
                LOG_WRN("Initial send failed, retrying");
                TRAN(st_connected);
            }
        } else {
            if (++sm->timer > 30) {
                if (sm->count >= 3) {
                    LOG_ERR("Send failed after 3 attempts");
                    Payload p;
                    k_msgq_get(&payload_queue_, &p, K_NO_WAIT);
                    k_event_post(&lora_response_event, LORA_SEND_ERROR_BIT);
                    TRAN(st_join);
                    return;
                }
                Payload payload;
                if (k_msgq_peek(&payload_queue_, &payload) == 0) {
                    LOG_WRN("No ACK, resending");
                    sm->timer = 0;
                    lora_flush(sm);
                    send_hex(sm, &payload);
                    ++sm->count;
                }
            }
        }
        break;
    }
    case EvType::Receive: {
        int res = lora_wait_for(sm, {
        "+CMSGHEX: ACK Received",
        "+CMSGHEX: Done",
        "+CMSGHEX: Length error",
    });
        if (res == 0) {
            LOG_INF("ACK received");
            last_send_count_ = sm->count;
            k_event_post(&lora_response_event, LORA_MESSAGE_SENT_BIT);
            Payload p;
            k_msgq_get(&payload_queue_, &p, K_NO_WAIT);
            TRAN(st_connected);
        } else if (res == 2) {
            LOG_ERR("Length error");
            Payload p;
            k_msgq_get(&payload_queue_, &p, K_NO_WAIT);
            k_event_post(&lora_response_event, LORA_SEND_ERROR_BIT);
            TRAN(st_connected);
        }
        break;
    }
    default:
        break;
    }
}
