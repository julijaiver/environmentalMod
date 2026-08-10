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
        void subscribe(struct k_msgq *q);
        int publish(const T &msg); //int will return dropped msg count if eg.queue is full
};

extern template class Topic<ruuvi_data>;
extern template class Topic<teros12_data>;
extern template class Topic<solyx14_data>;
extern template class Topic<solinst_data>;