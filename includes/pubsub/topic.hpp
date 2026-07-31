#pragma once

#include <zephyr/kernel.h>
#include "data_types.hpp"

template<typename T, int MAX_SUBS=5>
class Topic {
    private:
        struct k_msgq *subs_[MAX_SUBS]{};
        int count_ = 0;

    public:
        void subscribe(struct k_msgq *q);
        int publish(const T &msg); //int will return dropped msg count if eg.queue is full
};

extern template class Topic<ruuvi_data>;
extern template class Topic<teros12_data>;
extern template class Topic<solyx14_data>;
extern template class Topic<solinst_data>;