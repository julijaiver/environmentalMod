#include "pubsub/topic.hpp"

template<typename T, int MAX_SUBS>
void Topic<T, MAX_SUBS>::subscribe(struct k_msgq *q)
{
    if (count_ < MAX_SUBS) {
        subs_[count_++] = q; //adding a new subscriber queue to array
    }
}

template<typename T, int MAX_SUBS>
int Topic<T, MAX_SUBS>::publish(const T &msg)
{
    int dropped = 0;
    for (int i=0; i<count_; ++i) {
        if (k_msgq_put(subs_[i], &msg, K_NO_WAIT) != 0) {
            ++dropped;
        }
    }
    return dropped;
}

template class Topic<ruuvi_data>;
template class Topic<teros12_data>;
template class Topic<solyx14_data>;
template class Topic<solinst_data>;