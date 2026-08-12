#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(lora_transport);

#include <zephyr/kernel.h>

#include "transport/lora_transport.hpp"

LoRaTransport::LoRaTransport(LoRaStateMachine &sm)
                            : sm_(sm)
{
}

//default send just sends a data message
int LoRaTransport::send(const uint8_t *buf, int len)
{
    return send_on_port(buf, len, LoRaStateMachine::DATA_MSG_PORT);
}

int LoRaTransport::send_log(const uint8_t *buf, int len)
{
    return send_on_port(buf, len, LoRaStateMachine::LOG_MSG_PORT);
}

int LoRaTransport::send_on_port(const uint8_t *buf, int len, int port)
{
    int rc = sm_.queue_payload(buf, len, port);
    if (rc != 0) {
        return rc;
    }
    //wait for ack or for error (3 retries 30s each so 95 second timeout to account for those)
    return sm_.wait_for_response(LoRaStateMachine::LORA_MESSAGE_SENT_BIT | LoRaStateMachine::LORA_SEND_ERROR_BIT, K_SECONDS(95));
}

int LoRaTransport::max_payload()
{
    return sm_.get_max_payload_len();
}
