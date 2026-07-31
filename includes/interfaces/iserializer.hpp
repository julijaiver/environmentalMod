#pragma once

#include <stdint.h>
//abstract interface class for json and binary serializers
class ISerializer {
    public:
        //function for different data packing(protobuf and json)
        virtual int pack(uint8_t *buf, int max_len) = 0;
        //how many items are queued for packing
        virtual int pending() = 0;
        virtual ~ISerializer() = default;
};
