#pragma once

//definitions of sensors topics that are used throughout the data pipeline
#include "pubsub/topic.hpp"

extern Topic<teros12_data> teros_topic;
extern Topic<solyx14_data> solyx_topic;
extern Topic<ruuvi_data> ruuvi_topic;
extern Topic<solinst_data> solinst_topic;