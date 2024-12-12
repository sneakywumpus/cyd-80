/*
 * Z80SIM  -  a Z80-CPU simulator
 *
 * Copyright (C) 2024 by Udo Munk & Thomas Eberhardt
 *
 * This is the main program for a ESP32-2432S028R board,
 * substitutes z80core/simmain.c
 *
 * History:
 * 28-APR-2024 implemented first release of Z80 emulation
 * 09-MAY-2024 test 8080 emulation
 * 27-MAY-2024 add access to files on MicroSD
 * 28-MAY-2024 implemented boot from disk images with some OS
 * 31-MAY-2024 use USB UART
 * 09-JUN-2024 implemented boot ROM
 */

/* Raspberry SDK and FatFS includes */
#include <stdio.h>
#include <string.h>

#include "esp_system.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/uart_vfs.h"

/* Project includes */
#include "sim.h"
#include "simdefs.h"
#include "simglb.h"
#include "simcfg.h"
#include "simmem.h"
#include "simcore.h"
#include "simport.h"
#include "simio.h"
#ifdef WANT_ICE
#include "simice.h"
#endif

#include "disks.h"
#include "cydsim.h"

static const char *TAG = "main";

#define BS  0x08 /* backspace */
#define DEL 0x7f /* delete */

/* CPU speed */
int speed = CPU_SPEED;

void app_main(void)
{
	esp_err_t ret;
	char s[2];
	gpio_config_t led_conf = {
		.intr_type = GPIO_INTR_DISABLE,
		.mode = GPIO_MODE_OUTPUT,
		.pin_bit_mask = (1ULL << LED_RED_PIN) |
				(1ULL << LED_GREEN_PIN) |
				(1ULL << LED_BLUE_PIN),
		.pull_down_en = 0,
		.pull_up_en = 0
	};

	/* turn off task watchdog timer for now */
	ret = esp_task_wdt_deinit();
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to turn off the Task Watchdog Timer.");
		abort();
	}

	/* configure LED's and turn them off */
	gpio_config(&led_conf);
	gpio_set_level(LED_RED_PIN, 1);
	gpio_set_level(LED_GREEN_PIN, 1);
	gpio_set_level(LED_BLUE_PIN, 1);

	/* initialize VFS & UART so we can use stdout/stdin */
	setvbuf(stdin, NULL, _IONBF, 0);
	setvbuf(stdout, NULL, _IONBF, 0);
	/* install UART driver for interrupt-driven reads and writes */
	ret = uart_driver_install((uart_port_t) CONFIG_ESP_CONSOLE_UART_NUM,
				  256, 0, 0, NULL, 0);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to install UART driver.");
		abort();
	}
	/* tell VFS to use UART driver */
	uart_vfs_dev_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);
	/* setup CR/LF handling */
	uart_vfs_dev_port_set_rx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM,
					      ESP_LINE_ENDINGS_LF);
	uart_vfs_dev_port_set_tx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM,
					      ESP_LINE_ENDINGS_CRLF);

	init_cpu();		/* initialize CPU */
	init_disks();		/* initialize disk drives */
	init_memory();		/* initialize memory configuration */
	init_io();		/* initialize I/O devices */

	/* print banner */
	printf("\fZ80pack release %s, %s\n", RELEASE, COPYR);
	printf("%s release %s\n", USR_COM, USR_REL);
	printf("%s\n\n", USR_CPR);

	config();		/* configure the machine */

	f_flag = speed;		/* setup speed of the CPU */
	tmax = speed * 10000;	/* theoretically */

	PC = 0xff00;		/* power on jump into the boot ROM */

	/* run the CPU with whatever is in memory */
#ifdef WANT_ICE
	ice_cmd_loop(0);
#else
	run_cpu();
#endif

	exit_disks();		/* stop disk drives */

#ifndef WANT_ICE
	putchar('\n');
	report_cpu_error();	/* check for CPU emulation errors and report */
	report_cpu_stats();	/* print some execution statistics */
#endif
	puts("\nPress any key to restart CPU");
	get_cmdline(s, 2);

	fflush(stdout);
	esp_restart();
}

/*
 * Read an ICE or config command line of maximum length len - 1
 * from the terminal. For single character requests (len == 2),
 * returns immediately after input is received.
 */
int get_cmdline(char *buf, int len)
{
	int c, i = 0;

	for (;;) {
		c = getchar();
		if ((c == BS) || (c == DEL)) {
			if (i >= 1) {
				putchar(BS);
				putchar(' ');
				putchar(BS);
				i--;
			}
		} else if (c != '\r') {
			if (i < len - 1) {
				buf[i++] = c;
				putchar(c);
				if (len == 2)
					break;
			}
		} else {
			break;
		}
	}
	buf[i] = '\0';
	putchar('\n');
	return 0;
}
