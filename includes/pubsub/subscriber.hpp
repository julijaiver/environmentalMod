#pragma once

#include <zephyr/kernel.h>
#include "pubsub/topic.hpp"

// Subscriber class initializes own queue and adds it to array of subs in Topic by subscribing.
// Then can get messages from that queue and do other ops
template<typename T, int NUM_MSG = 16>
class Subscriber
{
    private:
    //alignas to align buffer start address to T
        alignas(T) uint8_t buf_[sizeof(T) * NUM_MSG];
        struct k_msgq msgq_;

    public:
        void init(Topic<T> &t)
        {
            k_msgq_init(&msgq_, reinterpret_cast<char *>(buf_), sizeof(T), NUM_MSG);
            t.subscribe(&msgq_);
        }

        bool get(T &out, k_timeout_t timeout = K_FOREVER)
        {
            return k_msgq_get(&msgq_, &out, timeout) == 0;
        }

        bool peek(T &out)
        {
            return k_msgq_peek(&msgq_, &out) == 0;
        }

        void remove()
        {
            T temp;
            k_msgq_get(&msgq_, &temp, K_NO_WAIT);
        }

        int pending()
        {
            return static_cast<int>(k_msgq_num_used_get(&msgq_));
        }
};
