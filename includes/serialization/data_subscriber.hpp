#pragma once

#include "interfaces/iserializer.hpp"
#include "pubsub/subscriber.hpp"
#include "data_types.hpp"

//binary and json serializers derived from this class, 
//owns subscriber queues that serializers use in pack()

class DataSubscriber : public ISerializer {
protected:
    Subscriber<ruuvi_data, 16> ruuvi_sub_;
    Subscriber<teros12_data, 16> teros_sub_;
    Subscriber<solyx14_data, 16> solyx_sub_;
    Subscriber<solinst_data, 16> solinst_sub_;

public:
    DataSubscriber(Topic<ruuvi_data> &ruuvi_topic, 
        Topic<teros12_data> &teros_topic, 
        Topic<solyx14_data> &solyx_topic,
        Topic<solinst_data> &solinst_topic)
    {
        ruuvi_sub_.init(ruuvi_topic);
        teros_sub_.init(teros_topic);
        solyx_sub_.init(solyx_topic);
        solinst_sub_.init(solinst_topic);
    }

    //function to use in CloudSender while loop for sending pending data
    int pending() override
    {
        return ruuvi_sub_.pending() + teros_sub_.pending() + solyx_sub_.pending() + solinst_sub_.pending();
    }
};
