#ifndef LORA_H_INCLUDED_
#define LORA_H_INCLUDED_

#include <stdint.h>

#define LORA_PAYLOAD_MAX_LEN 242

int lora_raw_write(const char *str);
int lora_raw_read(char *str, int max_len);

int lora_queue_payload(uint8_t *buf, int len);
int lora_wait_send_done(k_timeout_t timeout);

#endif