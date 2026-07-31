#pragma once

#include <zephyr/kernel.h>
#include "pubsub/topic.hpp"

template<typename T, int NUM_MSG = 16>
class Subscriber 
{
    private:
    //alignas to align buffer start address to T 
        alignas(T) uint8_t buf_[sizeof(T) * NUM_MSG];
        struct k_msgq msgq_;
    
    public:
        void init(Topic<T> &t);
        bool get(T &out, k_timeout_t timeout= K_FOREVER);
        bool peek(T &out);
        void remove();
        int pending();
};

extern template class Subscriber<ruuvi_data, 16>;
extern template class Subscriber<teros12_data, 16>;
extern template class Subscriber<solyx14_data, 16>;
extern template class Subscriber<solinst_data, 16>;

