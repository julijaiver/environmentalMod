#ifndef NV_PARAMS_H_INCLUDED_
#define NV_PARAMS_H_INCLUDED_

#include <zephyr/bluetooth/bluetooth.h>

int nv_params_init(void);
int is_listed(const bt_addr_t *bta);
int get_tag_mask(void);
int nvs_write_tags(void);
int nvs_clear_tags(void);
int nvs_read_tags(void);
int nvs_print_tags(void);
int nvs_tag_add(const char *tagstr);
int nvs_get_lora_appkey(char *key);
int nvs_set_lora_appkey(const char *key);

int nvs_get_interval(void);

#endif