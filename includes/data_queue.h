#ifndef DATA_QUEUE_H_INCLUDED_
#define DATA_QUEUE_H_INCLUDED_

#include "data_types.h"

#define DATA_QUEUE_LENGTH   256

int data_put(struct sensor_data *data);
void cloud_send_notify(struct k_timer *timer);

#endif