#include <zephyr/kernel.h>
#include "uart.h"
#include "modem.h"
#include "cloud.h"
// TODO: Try to shrink main function
int main(void)
{
	int uart_ret = uart_init();
	if (uart_ret < 0) {
		printk("UART initialization failed: %d\n", uart_ret);
	} else {
		printk("UART initialization successful\n");

		printk("Initializing modem...\n");

		// TODO: add timer to send only once a day
		modem_status_t modem_ret = initialize_modem();
		if(modem_ret != MODEM_SUCCESS) {
			printk("Modem initialization failed: %s (%d)\n", modem_status_to_string(modem_ret), modem_ret);
		} else {
			printk("Modem initialization successful\n");

			// TODO: move these to a separate function
			
			const char *data = "{\"data\":\"value\"}";

			printk("Sending HTTP POST request to %s\n", CLOUD_HOST);
			modem_ret = send_http_post(CLOUD_HOST, data);
			if(modem_ret != MODEM_SUCCESS) {
				printk("HTTP POST failed: %s (%d)\n", modem_status_to_string(modem_ret), modem_ret);
			} else {
				printk("HTTP POST successful\n");
			}
		}
	}

	// TODO: take measurements every 5 minutes

	return 0;
}
