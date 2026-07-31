#include "pubsub/publisher.hpp"

template<typename T>
Publisher<T>::Publisher(Topic<T> &t)
            :topic_(t)
{}

template<typename T>
void Publisher<T>::publish(const T &msg)
{
    topic_.publish(msg);
}

template class Publisher<ruuvi_data>;
template class Publisher<solyx14_data>;
template class Publisher<teros12_data>;
template class Publisher<solinst_data>;