#pragma once

#include "sensor_serializer.hpp"

//json serializer for 4G module, produces JSON from subscriber queues
class JsonSerializer : public SensorSerializer {
    public:
        using SensorSerializer::SensorSerializer;
        int pack(uint8_t *buf, int max_len) override;
};