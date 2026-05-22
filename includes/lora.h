#ifndef LORA_H_INCLUDED_
#define LORA_H_INCLUDED_

#include <stdint.h>
#include <zephyr/kernel.h>

                               
extern struct k_event lora_response_event;

#define LORA_LEN_READY_BIT BIT(0)
#define LORA_MESSAGE_SENT_BIT BIT(1)
#define LORA_SEND_ERROR_BIT BIT(2)

#define LORA_PAYLOAD_MAX_LEN 242
int lora_raw_write(const char *str);
int lora_raw_read(char *str, int max_len);

int lora_queue_payload(uint8_t *buf, int len);
int lora_get_max_payload_len(void);
int lora_get_last_send_count(void);

#endif