#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/version.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/debug/thread_analyzer.h>

#include <stdlib.h>
#include <ctype.h>

#include "nv_params.h"
#include "data_queue.h"


static int cmd_nvs_read_tags(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(sh);
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return nvs_read_tags();
}


static int cmd_nvs_tag_add(const struct shell *sh, size_t argc, char **argv)
{
    if(argc < 2) return -1;
   
	return nvs_tag_add(argv[1]);
}


static int cmd_nvs_print_tags(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(sh);
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return nvs_print_tags();
}


static int cmd_nvs_clear_tags(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(sh);
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	return nvs_clear_tags();
}


static int cmd_nvs_write_tags(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(sh);
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
    return nvs_write_tags();
}


SHELL_STATIC_SUBCMD_SET_CREATE(sub_tags,
    SHELL_CMD(read, NULL, "Read tags from NVS.", cmd_nvs_read_tags),
	SHELL_CMD(add, NULL, "Add a new tag.", cmd_nvs_tag_add),
	SHELL_CMD(list, NULL, "Print list of tags.", cmd_nvs_print_tags),
    SHELL_CMD(clear, NULL, "Erase all tags.", cmd_nvs_clear_tags),
    SHELL_CMD(write, NULL, "Write tags to NVS.", cmd_nvs_write_tags),
	SHELL_SUBCMD_SET_END /* Array terminated. */
);

SHELL_CMD_REGISTER(tag, &sub_tags, "BLE tag commands.", NULL);

static int cmd_trigger_transmit(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(sh);
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	cloud_send_notify(NULL);

	return 0;
}
SHELL_CMD_REGISTER(transmit, NULL, "Trigger data transmit.", cmd_trigger_transmit);

static int cmd_thread_info(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(sh);
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	thread_analyzer_print(0);

	return 0;
}
SHELL_CMD_REGISTER(stat, NULL, "Print thread statistics", cmd_thread_info);

void boot_halt(void);
void boot_continue(void);

static int cmd_boot_halt(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(sh);
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	boot_halt();

	return 0;
}
SHELL_CMD_REGISTER(stop, NULL, "Stop booting", cmd_boot_halt);

static int cmd_boot_continue(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(sh);
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	boot_continue();

	return 0;
}
SHELL_CMD_REGISTER(continue, NULL, "Continue booting", cmd_boot_continue);
