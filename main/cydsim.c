/*
 * Z80SIM  -  a Z80-CPU simulator
 *
 * Copyright (C) 2024-2025 by Udo Munk & Thomas Eberhardt
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

/* ESP-IDF includes */
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "esp_system.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/uart_vfs.h"

#include "gpio.h"

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

#ifdef WANT_ICE
#include "driver/gptimer.h"
#endif

#include "disks.h"
#include "cydsim.h"

static const char *TAG = "main";

#ifdef WANT_ICE
static void cydsim_ice_cmd(char *cmd, WORD *wrk_addr);
static void cydsim_ice_help(void);
#endif

#define BS  0x08 /* ASCII backspace */
#define DEL 0x7f /* ASCII delete */

/* CPU speed */
int speed = CPU_SPEED;

static QueueHandle_t uart0_queue;

/*
 *	Handle UART BREAK event
 */
static void uart_event_task(void *pvParameters)
{
	uart_event_t event;

	while (true) {
		/* wait for UART event */
		if (xQueueReceive(uart0_queue, (void *)&event,
				  (TickType_t) portMAX_DELAY)) {
			if (event.type == UART_BREAK) {
				cpu_error = USERINT;
				cpu_state = ST_STOPPED;
			}
		}
	}
	vTaskDelete(NULL);
}

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
				  256, 0, 10, &uart0_queue, 0);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to install UART driver.");
		abort();
	}
	/* create a task to handle UART BREAK event */
	xTaskCreate(uart_event_task, "uart_event_task", 1024, NULL, 12, NULL);
	/* tell VFS to use UART driver */
	uart_vfs_dev_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);
	/* setup CR/LF handling */
	uart_vfs_dev_port_set_rx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM,
					      ESP_LINE_ENDINGS_LF);
	uart_vfs_dev_port_set_tx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM,
					      ESP_LINE_ENDINGS_CRLF);

	init_disks();		/* initialize disk drives */

	/* print banner */
	printf("\fZ80pack release %s, %s\n", RELEASE, COPYR);
	printf("%s release %s\n", USR_COM, USR_REL);
	printf("%s\n\n", USR_CPR);

	init_cpu();		/* initialize CPU */
	PC = 0xff00;		/* power on jump into the boot ROM */
	init_memory();		/* initialize memory configuration */
	init_io();		/* initialize I/O devices */
	config();		/* configure the machine */

	f_value = speed;	/* setup speed of the CPU */
	if (f_value)
		tmax = speed * 10000;	/* theoretically */
	else
		tmax = 100000;	/* for periodic CPU accounting updates */

	/* run the CPU with whatever is in memory */
#ifdef WANT_ICE
	ice_cust_cmd = cydsim_ice_cmd;
	ice_cust_help = cydsim_ice_help;
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
bool get_cmdline(char *buf, int len)
{
	int i = 0;
	char c;

	while (true) {
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
	return true;
}

#ifdef WANT_ICE

/*
 *	This function is the callback for the alarm.
 *	The CPU emulation is stopped here.
 */
static bool timeout(gptimer_handle_t timer,
		    const gptimer_alarm_event_data_t *edata, void *user_data)
{
	UNUSED(timer);
	UNUSED(edata);
	UNUSED(user_data);

	cpu_state = ST_STOPPED;
	return false;
}

static void cydsim_ice_cmd(char *cmd, WORD *wrk_addr)
{
	char *s;
	const char *t;
	BYTE save[3];
	WORD save_PC;
	Tstates_t T0;
	unsigned freq;
#ifdef WANT_HB
	bool save_hb_flag;
#endif
	gptimer_handle_t gptimer = NULL;
	gptimer_config_t timer_config = {
		.clk_src = GPTIMER_CLK_SRC_DEFAULT,
		.direction = GPTIMER_COUNT_UP,
		.resolution_hz = 1000000 /* 1 MHz */
	};
	gptimer_event_callbacks_t cbs = {
		.on_alarm = timeout
	};
	gptimer_alarm_config_t alarm_config = {
		.alarm_count = 3000000 /* period = 3 s */
	};

	switch (tolower((unsigned char) *cmd)) {
	case 'c':
		/*
		 *	Calculate the clock frequency of the emulated CPU:
		 *	into memory locations 0000H to 0002H the following
		 *	code will be stored:
		 *		LOOP: JP LOOP
		 *	It uses 10 T states for each execution. A 3 second
		 *	alarm is set and then the CPU started. For every JP
		 *	the T states counter is incremented by 10 and after
		 *	the timer is down and stops the emulation, the clock
		 *	speed of the CPU in MHz is calculated with:
		 *		f = (T - T0) / 3000000
		 */

#ifdef WANT_HB
		save_hb_flag = hb_flag;
		hb_flag = false;
#endif
		save[0] = getmem(0x0000); /* save memory locations */
		save[1] = getmem(0x0001); /* 0000H - 0002H */
		save[2] = getmem(0x0002);
		putmem(0x0000, 0xc3);	/* store opcode JP 0000H at address */
		putmem(0x0001, 0x00);	/* 0000H */
		putmem(0x0002, 0x00);
		save_PC = PC;		/* save PC */
		PC = 0;			/* set PC to this code */
		T0 = T;			/* remember start clock counter */
					/* setup 3 second alarm timer */
		ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));
		ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, NULL));
		ESP_ERROR_CHECK(gptimer_enable(gptimer));
		ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config));
		ESP_ERROR_CHECK(gptimer_start(gptimer));
		run_cpu();		/* start CPU */
					/* cleanup alarm timer */
		ESP_ERROR_CHECK(gptimer_stop(gptimer));
		ESP_ERROR_CHECK(gptimer_disable(gptimer));
		ESP_ERROR_CHECK(gptimer_del_timer(gptimer));
		PC = save_PC;		/* restore PC */
		putmem(0x0000, save[0]); /* restore memory locations */
		putmem(0x0001, save[1]); /* 0000H - 0002H */
		putmem(0x0002, save[2]);
#ifdef WANT_HB
		hb_flag = save_hb_flag;
#endif
#ifndef EXCLUDE_Z80
		if (cpu == Z80)
			t = "JP";
#endif
#ifndef EXCLUDE_I8080
		if (cpu == I8080)
			t = "JMP";
#endif
		if (cpu_error == NONE) {
			freq = (unsigned) ((T - T0) / 30000);
			printf("CPU executed %" PRIu64 " %s instructions "
			       "in 3 seconds\n", (T - T0) / 10, t);
			printf("clock frequency = %u.%02u MHz\n",
			       freq / 100, freq % 100);
		} else
			puts("Interrupted by user");
		break;

	case 'r':
		cmd++;
		while (isspace((unsigned char) *cmd))
			cmd++;
		for (s = cmd; *s; s++)
			*s = toupper((unsigned char) *s);
		if (load_file(cmd))
			*wrk_addr = PC = 0;
		break;

	case '!':
		cmd++;
		while (isspace((unsigned char) *cmd))
			cmd++;
		if (strcasecmp(cmd, "ls") == 0)
			list_files(SD_MNTDIR "/CODE80", "*.BIN");
		else
			puts("what??");
		break;

	default:
		puts("what??");
		break;
	}
}

static void cydsim_ice_help(void)
{
	puts("c                         measure clock frequency");
	puts("r filename                read file (without .BIN) into memory");
	puts("! ls                      list files");
}

#endif
