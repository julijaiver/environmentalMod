#pragma once

#include "interfaces/itransport.hpp"
#include "transport/lora_statemachine.hpp"

// ITransport for LoRa
class LoRaTransport : public ITransport {
    LoRaStateMachine &sm_;

    //different ports for log and for data messages
    int send_on_port(const uint8_t *buf, int len, int port);

public:
    explicit LoRaTransport(LoRaStateMachine &sm);

    int  send(const uint8_t *buf, int len) override;
    int  send_log(const uint8_t *buf, int len) override;
    int  max_payload() override;
};

