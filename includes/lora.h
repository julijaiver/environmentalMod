#ifndef LORA_H_INCLUDED_
#define LORA_H_INCLUDED_

int lora_raw_write(const char *str);
int lora_raw_read(char *str, int max_len);

#endif