#pragma once
// interface class for different transport methods (LoRa and 4G)

#include <stdint.h>

class ITransport {
    public:
        virtual int send(const uint8_t *buf, int len) = 0;  
        virtual int send_log(const uint8_t *buf, int len) = 0;  
        virtual int max_payload() = 0;
        virtual int connect() {return 0;}
        virtual int disconnect() { return 0; }
        virtual ~ITransport() = default;
};