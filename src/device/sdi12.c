#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/shell/shell.h>
#include <zephyr/drivers/uart_pipe.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/modem/backend/uart.h>
#include <zephyr/modem/pipe.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sdi12, CONFIG_MODEM_MODULES_LOG_LEVEL);

#include <stdio.h>
#include <string.h>
#include "sdi12.h"

#define SDI12_RECEIVE_EVENT 1

#define DEV_SDI12 DEVICE_DT_GET(DT_NODELABEL(uart1))

#define UART_RX_BUF_SIZE 128
#define UART_TX_BUF_SIZE 128
#define CONSOLE_RX_BUF_SIZE 128

/* Buffers for modem backend */
struct modem_data
{
	struct
	{
		uint8_t uart_rx[UART_RX_BUF_SIZE];
		uint8_t uart_tx[UART_TX_BUF_SIZE];
	} buffers;

	struct modem_backend_uart mdm_uart_backend;
	struct modem_pipe *mdm_uart_pipe;

	struct k_event rcv_event;
};

static struct modem_data data;

#define ZEPHYR_USER_NODE DT_PATH(zephyr_user)
static const struct gpio_dt_spec break_ctrl = GPIO_DT_SPEC_GET(ZEPHYR_USER_NODE, break_pin_gpios);

// nRF uart can't do 7,e,1 so we need to set 8,n,1 and add the parity bit to the data
// UART config is set in board overlay
char add_even_parity(char ch)
{
	const char parity_e[] = {
		0x00, 0x81, 0x82, 0x03, 0x84, 0x05, 0x06, 0x87,
		0x88, 0x09, 0x0A, 0x8B, 0x0C, 0x8D, 0x8E, 0x0F,
		0x90, 0x11, 0x12, 0x93, 0x14, 0x95, 0x96, 0x17,
		0x18, 0x99, 0x9A, 0x1B, 0x9C, 0x1D, 0x1E, 0x9F,
		0xA0, 0x21, 0x22, 0xA3, 0x24, 0xA5, 0xA6, 0x27,
		0x28, 0xA9, 0xAA, 0x2B, 0xAC, 0x2D, 0x2E, 0xAF,
		0x30, 0xB1, 0xB2, 0x33, 0xB4, 0x35, 0x36, 0xB7,
		0xB8, 0x39, 0x3A, 0xBB, 0x3C, 0xBD, 0xBE, 0x3F,
		0xC0, 0x41, 0x42, 0xC3, 0x44, 0xC5, 0xC6, 0x47,
		0x48, 0xC9, 0xCA, 0x4B, 0xCC, 0x4D, 0x4E, 0xCF,
		0x50, 0xD1, 0xD2, 0x53, 0xD4, 0x55, 0x56, 0xD7,
		0xD8, 0x59, 0x5A, 0xDB, 0x5C, 0xDD, 0xDE, 0x5F,
		0x60, 0xE1, 0xE2, 0x63, 0xE4, 0x65, 0x66, 0xE7,
		0xE8, 0x69, 0x6A, 0xEB, 0x6C, 0xED, 0xEE, 0x6F,
		0xF0, 0x71, 0x72, 0xF3, 0x74, 0xF5, 0xF6, 0x77,
		0x78, 0xF9, 0xFA, 0x7B, 0xFC, 0x7D, 0x7E, 0xFF};
	return parity_e[ch & 0x7F];
}

inline char remove_even_parity(char ch)
{
	return ch & 0x7F;
}

void sdi12_send_break(void)
{
	k_msleep(50);
	gpio_pin_set_dt(&break_ctrl, 1);
	k_msleep(15);
	gpio_pin_set_dt(&break_ctrl, 0);
	k_msleep(10);
}

/* Callback when modem pipe receives data */
static void modem_pipe_event_handler(struct modem_pipe *pipe, enum modem_pipe_event event,
									 void *user_data)
{
	struct modem_data *usr = (struct modem_data *)user_data;

	switch (event)
	{
	case MODEM_PIPE_EVENT_RECEIVE_READY:
		LOG_DBG("rcv ready");
		k_event_post(&usr->rcv_event, SDI12_RECEIVE_EVENT);
		break;

	case MODEM_PIPE_EVENT_TRANSMIT_IDLE:
		LOG_DBG("transmit idle");
		/* Can send more data if available */
		break;

	default:
		LOG_DBG("modem event: %d", event);
		break;
	}
}

static int init_modem_pipe(void)
{
	int ret;

	const struct modem_backend_uart_config uart_backend_config = {
		.uart = DEV_SDI12,
		.receive_buf = data.buffers.uart_rx,
		.receive_buf_size = sizeof(data.buffers.uart_rx),
		.transmit_buf = data.buffers.uart_tx,
		.transmit_buf_size = sizeof(data.buffers.uart_tx),
	};
	k_event_init(&data.rcv_event);

	data.mdm_uart_pipe = modem_backend_uart_init(&data.mdm_uart_backend, &uart_backend_config);
	if (data.mdm_uart_pipe == NULL)
	{
		LOG_ERR("Failed to initialize modem backend");
		return -1;
	}

	modem_pipe_attach(data.mdm_uart_pipe, modem_pipe_event_handler, &data);

	ret = modem_pipe_open(data.mdm_uart_pipe, K_MSEC(100));
	if (ret < 0)
	{
		LOG_ERR("Failed to open modem pipe");
		return ret;
	}

	LOG_INF("Modem pipe initialized and opened");
	return 0;
}

int sdi12_init(void)
{
	int ret;

	LOG_INF("SDI12 starting...");

	if (!gpio_is_ready_dt(&break_ctrl))
	{
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&break_ctrl, GPIO_OUTPUT_INACTIVE);
	if (ret < 0)
	{
		return -1;
	}

	/* Verify modem device is ready */
	if (!device_is_ready(DEV_SDI12))
	{
		LOG_ERR("SDI12 UART device not ready");
		return -ENODEV;
	}

	/* Initialize modem pipe */
	ret = init_modem_pipe();
	if (ret < 0)
	{
		LOG_ERR("Failed to initialize modem pipe: %d", ret);
		return ret;
	}

	LOG_INF("SDI12 pipe initialized");

	return 0;
}

int sdi12_flush(void)
{
	int count = 0;
	int rv = 0;
	char dummy[32];

	k_event_clear(&data.rcv_event, SDI12_RECEIVE_EVENT);
	do
	{
		rv = modem_pipe_receive(data.mdm_uart_pipe, dummy, sizeof(dummy));
		if (rv > 0)
		{
			LOG_INF("flushed: %d", rv);
			count += rv;
		}
	} while (rv > 0);

	return count;
}

int sdi12_wait_for(char *buffer, int size, const char *expect)
{
	int ret = 0;
	int pos = 0;
	buffer[0] = 0; // ensure proper termination

	ret = k_event_wait(&data.rcv_event, SDI12_RECEIVE_EVENT, false, K_MSEC(3000));

	/* Read from modem pipe */
	while (ret > 0)
	{
		k_event_clear(&data.rcv_event, SDI12_RECEIVE_EVENT);
		int remain = size - pos - 1; // leave spce for NUL
		char *buf = buffer + pos;
		if (remain < 1)
			return -2; // buffer full

		ret = modem_pipe_receive(data.mdm_uart_pipe, buf, remain);
		if (ret > 0)
		{
			for (int i = 0; i < ret; ++i)
				buf[i] = remove_even_parity(buf[i]);
			buf[ret] = 0;
			pos += ret;
			LOG_HEXDUMP_DBG(buf, ret, "RCV:");
			if (strchr(buffer, '\n'))
			{
				LOG_INF("Response: %s", buffer);
				// return if expected string was found or NULL pointer was passed
				if (expect == NULL || strstr(buffer, expect))
					return strlen(buffer);
			}
			// wait for 1000 ms for more data
			ret = k_event_wait(&data.rcv_event, SDI12_RECEIVE_EVENT, false, K_MSEC(1000));
		}
	}
	LOG_INF("No response: %s", buffer);

	return ret;
}

int sdi12_cmd(const char *cmd, bool send_break)
{
	int len = strlen(cmd);
	char buf[len + 1];
	for (int i = 0; i < len; ++i)
		buf[i] = add_even_parity(cmd[i]);
	buf[len] = '\0';

	if (send_break)
		sdi12_send_break();

	sdi12_flush();

	return modem_pipe_transmit(data.mdm_uart_pipe, buf, len);
}
