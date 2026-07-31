#include "pubsub/subscriber.hpp"

template<typename T, int NUM_MSG>
void Subscriber<T, NUM_MSG>::init(Topic<T> &t)
{
    //initializing queue when calling init func in main & subscribing to topic (adding own queue to topic array)
    //cast to char* to match msgq init func
    k_msgq_init(&msgq_, reinterpret_cast<char *>(buf_), sizeof(T), NUM_MSG);
    t.subscribe(&msgq_);
}

template<typename T, int NUM_MSG>
bool Subscriber<T, NUM_MSG>::get(T &out, k_timeout_t timeout)
{
    return k_msgq_get(&msgq_, &out, timeout) == 0;
}

template<typename T, int NUM_MSG>
bool Subscriber<T, NUM_MSG>::peek(T &out) 
{
    return k_msgq_peek(&msgq_, &out) == 0;
}

template<typename T, int NUM_MSG>
void Subscriber<T, NUM_MSG>::remove()
{
    T temp;
    k_msgq_get(&msgq_, &temp, K_NO_WAIT);
}

template<typename T, int NUM_MSG>
int Subscriber<T, NUM_MSG>::pending()
{
    return static_cast<int>(k_msgq_num_used_get(&msgq_));
}

template class Subscriber<ruuvi_data, 16>;
template class Subscriber<teros12_data, 16>;
template class Subscriber<solyx14_data, 16>;
template class Subscriber<solinst_data, 16>;