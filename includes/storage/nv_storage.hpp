#pragma once

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/fs/nvs.h>

class NvStorage {
    static constexpr int MAX_RUUVI_TAG = 10;
    struct nvs_fs fs_;
    bt_addr_t ruuvi_tags_[MAX_RUUVI_TAG];
    char lora_appkey_[33];

    static const bt_addr_t null_bt_;

public:
    NvStorage();

    int init();

    // ruuvi MAC list
    int read_tags();
    int write_tags();
    int add_tag(const char *tagstr);
    int print_tags();
    int clear_tags();

    int is_listed(const bt_addr_t *addr) const;
    int get_tag_mask() const;

    int get_appkey(char *key) const;
    int set_appkey(const char *key);
};
