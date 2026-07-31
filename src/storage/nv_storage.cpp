#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(nv_storage);

#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "storage/nv_storage.hpp"

#define NVS_PARTITION storage_partition
#define NVS_PARTITION_DEVICE FIXED_PARTITION_DEVICE(NVS_PARTITION)
#define NVS_PARTITION_OFFSET FIXED_PARTITION_OFFSET(NVS_PARTITION)

#define BLE_DEVICE_NV_ADDR 1
#define MEASUREMENT_INTERVAL_NV_ADDR 3
#define LORA_APPKEY_NV_ADDR 4

const bt_addr_t NvStorage::null_bt_ = {};

NvStorage::NvStorage()
{
    memset(ruuvi_tags_, 0, sizeof(ruuvi_tags_));
    memset(lora_appkey_, 0, sizeof(lora_appkey_));
}

int NvStorage::init()
{
    fs_.flash_device = NVS_PARTITION_DEVICE;
    if (!device_is_ready(fs_.flash_device)) {
        LOG_ERR("Flash device not ready");
        return -1;
    }
    fs_.offset = NVS_PARTITION_OFFSET;

    struct flash_pages_info info;
    int rc = flash_get_page_info_by_offs(fs_.flash_device, fs_.offset, &info);
    if (rc) {
        LOG_ERR("Unable to get flash page info: %d", rc);
        return rc;
    }
    fs_.sector_size  = info.size;
    fs_.sector_count = 3U;

    rc = nvs_mount(&fs_);
    if (rc) {
        LOG_ERR("Flash init failed: %d", rc);
        return rc;
    }

    nvs_read(&fs_, LORA_APPKEY_NV_ADDR, lora_appkey_, sizeof(lora_appkey_));

    return 0;
}

int NvStorage::read_tags()
{
    int rc = nvs_read(&fs_, BLE_DEVICE_NV_ADDR, &ruuvi_tags_, sizeof(ruuvi_tags_));
    LOG_DBG("read rv: %d", rc);
    LOG_HEXDUMP_DBG(&ruuvi_tags_, sizeof(ruuvi_tags_), "Tags:");

    for (int i = 0; i < MAX_RUUVI_TAG && bt_addr_cmp(&ruuvi_tags_[i], &null_bt_); ++i) {
        char addr_str[BT_ADDR_LE_STR_LEN];
        bt_addr_to_str(&ruuvi_tags_[i], addr_str, sizeof(addr_str));
        LOG_INF("%d, %s", i, addr_str);
    }
    return 0;
}

int NvStorage::write_tags()
{
    int rc = nvs_write(&fs_, BLE_DEVICE_NV_ADDR, &ruuvi_tags_, sizeof(ruuvi_tags_));
    LOG_INF("write rv: %d", rc);
    LOG_HEXDUMP_INF(&ruuvi_tags_, sizeof(ruuvi_tags_), "Tags:");
    return 0;
}

int NvStorage::add_tag(const char *tagstr)
{
    bt_addr_t new_bt = {};
    for (int i = BT_ADDR_SIZE - 1; i >= 0; --i) {
        unsigned int byte;
        int dist;
        if (sscanf(tagstr, "%x%n", &byte, &dist) != 1) {
            LOG_INF("parse error");
            return -2;
        }
        new_bt.val[i] = (uint8_t)byte;
        tagstr += dist;
        if (*tagstr == ':') ++tagstr;
    }

    {
        char addr_str[BT_ADDR_LE_STR_LEN];
        bt_addr_to_str(&new_bt, addr_str, sizeof(addr_str));
        LOG_INF("new: %s", addr_str);
    }

    if (is_listed(&new_bt)) {
        LOG_INF("Tag already exists");
        return 0;
    }

    int i = 0;
    while (i < MAX_RUUVI_TAG && bt_addr_cmp(&ruuvi_tags_[i], &null_bt_)) ++i;
    if (i < MAX_RUUVI_TAG) {
        ruuvi_tags_[i] = new_bt;
    } else {
        LOG_INF("List full");
    }
    return 0;
}

int NvStorage::print_tags()
{
    for (int i = 0; i < MAX_RUUVI_TAG; ++i) {
        if (bt_addr_cmp(&ruuvi_tags_[i], &null_bt_)) {
            char addr_str[BT_ADDR_LE_STR_LEN];
            bt_addr_to_str(&ruuvi_tags_[i], addr_str, sizeof(addr_str));
            LOG_INF("%d, %s", i, addr_str);
        }
    }
    return 0;
}

int NvStorage::clear_tags()
{
    for (int i = 0; i < MAX_RUUVI_TAG; ++i) {
        ruuvi_tags_[i] = null_bt_;
    }
    LOG_HEXDUMP_INF(&ruuvi_tags_, sizeof(ruuvi_tags_), "Tags:");
    return 0;
}

int NvStorage::is_listed(const bt_addr_t *addr) const
{
    for (int i = 0; i < MAX_RUUVI_TAG && bt_addr_cmp(&ruuvi_tags_[i], &null_bt_); ++i) {
        if (bt_addr_cmp(&ruuvi_tags_[i], addr) == 0) {
            char addr_str[BT_ADDR_LE_STR_LEN];
            bt_addr_to_str(&ruuvi_tags_[i], addr_str, sizeof(addr_str));
            LOG_INF("Match: %d, %s", i, addr_str);
            return 1 << i;
        }
    }
    return 0;
}

int NvStorage::get_tag_mask() const
{
    int mask = 0;
    for (int i = 0; i < MAX_RUUVI_TAG; ++i) {
        if (bt_addr_cmp(&ruuvi_tags_[i], &null_bt_) != 0) {
            mask |= 1 << i;
        }
    }
    return mask;
}

int NvStorage::get_appkey(char *key) const
{
    memcpy(key, lora_appkey_, sizeof(lora_appkey_));
    //return >0 if a key has been stored (first char non-zero)
    return (lora_appkey_[0] != '\0') ? (int)sizeof(lora_appkey_) : 0;
}

int NvStorage::set_appkey(const char *key)
{
    strncpy(lora_appkey_, key, sizeof(lora_appkey_) - 1);
    lora_appkey_[sizeof(lora_appkey_) - 1] = '\0';
    return nvs_write(&fs_, LORA_APPKEY_NV_ADDR, lora_appkey_, sizeof(lora_appkey_));
}
