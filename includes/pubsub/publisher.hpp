#pragma once

#include "pubsub/topic.hpp"

// Publisher class uses Topic's publish() to add its data to the subscriber queues
template<typename T>
class Publisher
{
    private:
        Topic<T> &topic_;

    public:
        Publisher(Topic<T> &t) : topic_(t) {}

        void publish(const T &msg)
        {
            topic_.publish(msg);
        }
};
