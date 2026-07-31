#pragma once

#include "pubsub/topic.hpp"

template<typename T>
class Publisher
{
    private:
        Topic<T> &topic_;

    public:
        explicit Publisher(Topic<T> &t);
        void publish(const T &msg);
};

extern template class Publisher<ruuvi_data>;
extern template class Publisher<solyx14_data>;
extern template class Publisher<teros12_data>;
extern template class Publisher<solinst_data>;



