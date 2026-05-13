#ifndef DATA_QUEUE_H_INCLUDED_
#define DATA_QUEUE_H_INCLUDED_

#include <zephyr/kernel.h>
#include "data_types.h"

#define DATA_QUEUE_LENGTH   256
#define CLOUD_SEND_RETRY_COUNT   3

// to use between lora and data_queue
#define CLOUD_WAKEUP_EVENT BIT(0)

int data_put(struct sensor_data *data);
void cloud_send_notify(struct k_timer *timer);

#endif