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
#include "sdi12.h"
#include "common.h"
#include "lora.h"


static int cmd_nvs_read_tags(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(sh);
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return nvs_read_tags();
}


static int cmd_nvs_tag_add(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(sh);
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


static int cmd_boot_take_sample(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(sh);
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	TAKE_SAMPLE_NOW();

	return 0;
}
SHELL_CMD_REGISTER(sample, NULL, "Trigger data sampling", cmd_boot_take_sample);


//int sdi12_cmd(const char *cmd, bool send_break);
//int sdi12_wait_for(char *buffer, int size, const char *expect);

static int cmd_sdi12_scan(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	for(int i = 0; i <= 9; ++i) {
		char cmd[]="0I!";
		cmd[0] = '0' + i;
		sdi12_cmd(cmd, true);
		char buffer[64];
		if(sdi12_wait_for(buffer, sizeof(buffer), NULL) > 3) {
			buffer[strcspn(buffer, "\r\n")] = 0;
			shell_print(sh, "[%s]", buffer);
		}
	}

	return 0;
}


static int cmd_sdi12_query(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	sdi12_cmd("?!", true);
	char buffer[32];
	if(sdi12_wait_for(buffer, sizeof(buffer), NULL) > 0) {
		buffer[strcspn(buffer, "\r\n")] = 0;
		shell_print(sh, "[%s]", buffer);
	}

	return 0;
}

static int cmd_sdi12_addr(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	if(argc < 3) return -1;
	if(!isdigit((int)argv[1][0])) return -2;
	if(!isdigit((int)argv[2][0])) return -3;

	char cmd[]="0A1!";
	cmd[0]=argv[1][0];
	cmd[2]=argv[2][0];

	sdi12_cmd(cmd, true);
	char buffer[32];
	if(sdi12_wait_for(buffer, sizeof(buffer), NULL) > 0) {
		buffer[strcspn(buffer, "\r\n")] = 0;
		shell_print(sh, "[%s]", buffer);
	}

	return 0;
}

static int cmd_sdi12_send(const struct shell *sh, size_t argc, char **argv)
{
	if(argc < 2) return -1;
	sdi12_cmd(argv[1], true);
	char buffer[32];
	if(sdi12_wait_for(buffer, sizeof(buffer), NULL) > 0) {
		buffer[strcspn(buffer, "\r\n")] = 0;
		shell_print(sh, "[%s]", buffer);
	}

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_sdi12,
    SHELL_CMD(scan, NULL, "Scan SDI-12 bus.", cmd_sdi12_scan),
    SHELL_CMD(query, NULL, "Query all addresses on the bus", cmd_sdi12_query),
	SHELL_CMD(addr, NULL, "Write new address to device.", cmd_sdi12_addr),
	SHELL_CMD(send, NULL, "Send command (any text) to SDI-12 bus.", cmd_sdi12_send),
	SHELL_SUBCMD_SET_END /* Array terminated. */
);

SHELL_CMD_REGISTER(sdi12, &sub_sdi12, "SDI-12 commands: scan, addr, send", NULL);


static int cmd_lora_read(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	char buffer[256] = {0};
	lora_raw_read(buffer, sizeof(buffer));

	shell_print(sh, "%s", buffer);

	return 0;
}

static int cmd_lora_write(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	if(argc < 2) return -1;
	lora_raw_write(argv[1]);
	lora_raw_write("\r\n");
	cmd_lora_read(sh, argc, argv);

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_lora,
    SHELL_CMD(write, NULL, "Write command to lora (no spaces allowed)", cmd_lora_write),
    SHELL_CMD(read, NULL, "Read pending data.", cmd_lora_read),
	SHELL_SUBCMD_SET_END /* Array terminated. */
);

SHELL_CMD_REGISTER(lora, &sub_lora, "lora commands: write, read", NULL);
