
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/shell/shell.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/fs/nvs.h>

#include <zephyr/bluetooth/bluetooth.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(nvparams, CONFIG_MODEM_MODULES_LOG_LEVEL);

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "nv_params.h"

static struct nvs_fs fs;

#define NVS_PARTITION		storage_partition
#define NVS_PARTITION_DEVICE	FIXED_PARTITION_DEVICE(NVS_PARTITION)
#define NVS_PARTITION_OFFSET	FIXED_PARTITION_OFFSET(NVS_PARTITION)

#define BLE_DEVICE_NV_ADDR  1
#define SDI12_DEVICE_NV_ADDR 2

#define MAX_RUUVI_TAG  10

static const bt_addr_t null_bt = {};

static bt_addr_t ruuvi_tags[MAX_RUUVI_TAG];


int nv_params_init(void) {
    int rc = 0;
	struct flash_pages_info info;

	/* define the nvs file system by settings with:
	 *	sector_size equal to the pagesize,
	 *	3 sectors
	 *	starting at NVS_PARTITION_OFFSET
	 */
	fs.flash_device = NVS_PARTITION_DEVICE;
	if (!device_is_ready(fs.flash_device)) {
		LOG_ERR("Flash device %s is not ready", fs.flash_device->name);
		return -1; // any better value??
	}
	fs.offset = NVS_PARTITION_OFFSET;
	rc = flash_get_page_info_by_offs(fs.flash_device, fs.offset, &info);
	if (rc) {
		LOG_ERR("Unable to get page info, rc=%d", rc);
		return rc;
	}
    //LOG_INF("Page size: %d", info.size);
	fs.sector_size = info.size;
	fs.sector_count = 3U;

	rc = nvs_mount(&fs);
	if (rc) {
		LOG_ERR("Flash Init failed, rc=%d", rc);
		return rc;
	}
    return 0;
}

int is_listed(const bt_addr_t *bta) {
    for(int i = 0; i < MAX_RUUVI_TAG && bt_addr_cmp(&ruuvi_tags[i], &null_bt); ++i) {
        if(bt_addr_cmp(&ruuvi_tags[i], bta)==0) {
            char addr_str[BT_ADDR_LE_STR_LEN];
            bt_addr_to_str(&ruuvi_tags[i], addr_str, sizeof(addr_str));
            LOG_INF("Match: %d, %s", i, addr_str);
            return 1 << i;
        }
        
    }

	return -1;
}

int get_tag_mask(void)
{
    int mask = 0;
    for(int i = 0; i < MAX_RUUVI_TAG; ++i) {
        if(bt_addr_cmp(&ruuvi_tags[i], &null_bt) != 0) {
            mask |= 1 << i;
        }
        
    }
    return mask;
}

int nvs_read_tags(void)
{
    int rc = nvs_read(&fs, BLE_DEVICE_NV_ADDR, &ruuvi_tags, sizeof(ruuvi_tags));
    LOG_INF("read rv: %d", rc);
    LOG_HEXDUMP_INF(&ruuvi_tags, sizeof(ruuvi_tags), "Tags:");

    for(int i = 0; i < MAX_RUUVI_TAG && bt_addr_cmp(&ruuvi_tags[i], &null_bt); ++i) {
        char addr_str[BT_ADDR_LE_STR_LEN];
        bt_addr_to_str(&ruuvi_tags[i], addr_str, sizeof(addr_str));
        LOG_INF("%d, %s", i, addr_str);
    }

	return 0;
}


int nvs_tag_add(const char *tagstr)
{
    bt_addr_t new_bt = {};

    for(int i = BT_ADDR_SIZE - 1; i >= 0; --i) {
        unsigned int byte;
        int dist;
        if(sscanf(tagstr, "%x%n", &byte, &dist) != 1) {
            LOG_INF("ugh");
            return -2;
        }
        new_bt.val[i] = byte;
        tagstr += dist;
        if(*tagstr == ':') ++tagstr;
    }

    {
        char addr_str[BT_ADDR_LE_STR_LEN];
        bt_addr_to_str(&new_bt, addr_str, sizeof(addr_str));
        LOG_INF("new: %s", addr_str);
    }

    if(is_listed(&new_bt)) {
        LOG_INF("Tag already exists");
        return 0;
    }

    int i = 0;
    while(i < MAX_RUUVI_TAG && bt_addr_cmp(&ruuvi_tags[i], &null_bt)) ++i;
    if(i < MAX_RUUVI_TAG) {
        ruuvi_tags[i] = new_bt;
    }
    else {
        LOG_INF("Unable to add - list is full");
    }
  
	return 0;
}


int nvs_print_tags(void)
{
    for(int i = 0; i < MAX_RUUVI_TAG; ++i) {
        if(bt_addr_cmp(&ruuvi_tags[i], &null_bt)) {
            char addr_str[BT_ADDR_LE_STR_LEN];
            bt_addr_to_str(&ruuvi_tags[i], addr_str, sizeof(addr_str));
            LOG_INF("%d, %s", i, addr_str);
        }
    }

	return 0;
}


int nvs_clear_tags(void)
{
    for(int i = 0; i < MAX_RUUVI_TAG; ++i) {
        ruuvi_tags[i] = null_bt;
    }

    LOG_HEXDUMP_INF(&ruuvi_tags, sizeof(ruuvi_tags), "Tags:");

	return 0;
}


int nvs_write_tags(void)
{
    int rc = nvs_write(&fs, BLE_DEVICE_NV_ADDR, &ruuvi_tags, sizeof(ruuvi_tags));
    LOG_INF("write rv: %d", rc);
    LOG_HEXDUMP_INF(&ruuvi_tags, sizeof(ruuvi_tags), "Tags:");

	return 0;
}


