#pragma once

#include "data_subscriber.hpp"

//nanopb serializer for LoRa, encodes subscriber queue data
class BinarySerializer : public DataSubscriber {
    public:
        using DataSubscriber::DataSubscriber;
        int pack(uint8_t *buf, int max_len) override;
};