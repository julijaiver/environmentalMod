#pragma once

#include "data_subscriber.hpp"

//json serializer for 4G module, produces JSON from subscriber queues
class JsonSerializer : public DataSubscriber {
    public:
        using DataSubscriber::DataSubscriber;
        int pack(uint8_t *buf, int max_len) override;
};