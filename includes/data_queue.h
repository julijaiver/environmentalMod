#ifndef DATA_QUEUE_H_INCLUDED_
#define DATA_QUEUE_H_INCLUDED_

#include "data_types.h"

#define DATA_QUEUE_LENGTH   128

int data_put(struct sensor_data *data);

#endif