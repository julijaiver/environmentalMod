#pragma once

#include "sensor_serializer.hpp"

//nanopb serializer for LoRa, encodes subscriber queue data
class BinarySerializer : public SensorSerializer {
    public:
        using SensorSerializer::SensorSerializer;
        int pack(uint8_t *buf, int max_len) override;
};