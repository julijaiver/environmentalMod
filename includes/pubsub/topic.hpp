#pragma once

#include <array>
#include <zephyr/kernel.h>
#include "data_types.hpp"

// Topic class holds array of subscriber queues, to which data is sent with publish() function

template<typename T, int MAX_SUBS=5>
class Topic {
    private:
        std::array<struct k_msgq *, MAX_SUBS> subs_{};
        int count_ = 0;

    public:
        void subscribe(struct k_msgq *q)
        {
            if (count_ < MAX_SUBS) {
                subs_[count_++] = q;
            }
        }

        int publish(const T &msg) //int will return dropped msg count if eg.queue is full
        {
            int dropped = 0;
            for (int i=0; i<count_; ++i) {
                if (k_msgq_put(subs_[i], &msg, K_NO_WAIT) != 0) {
                    ++dropped;
                }
            }
            return dropped;
        }
};
