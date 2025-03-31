/*
 * Z80SIM  -  a Z80-CPU simulator
 *
 * Copyright (C) 2024-2025 by Udo Munk & Thomas Eberhardt
 *
 * I/O simulation for cydsim
 *
 * History:
 * 28-APR-2024 all I/O implemented for a first release
 * 09-MAY-2024 improved so that it can run some MITS Altair software
 * 11-MAY-2024 allow us to configure the port 255 value
 * 27-MAY-2024 moved io_in & io_out to simcore
 * 31-MAY-2024 added hardware control port for z80pack machines
 * 08-JUN-2024 implemented system reset
 * 09-JUN-2024 implemented boot ROM
 * 29-JUN-2024 implemented banked memory
 */

/* ESP-IDF includes */
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/uart.h"

/* Project includes */
#include "sim.h"
#include "simdefs.h"
#include "simglb.h"
#include "simmem.h"
#include "simcore.h"
#include "simio.h"

#include "gpio.h"

#include "rtc80.h"
#include "sd-fdc.h"

static const char *TAG = "IO";

/*
 *	Forward declarations of the I/O functions
 *	for all port addresses.
 */
static BYTE sios_in(void), siod_in(void), mmu_in(void), hwctl_in(void);
static BYTE fpsw_in(void);
static void led_out(BYTE data), siod_out(BYTE data), mmu_out(BYTE data);
static void hwctl_out(BYTE data), fpsw_out(BYTE data), fpled_out(BYTE data);

static BYTE sio_last;	/* last character received */
       BYTE fp_value;	/* port 255 value, can be set from ICE or config() */
static BYTE hwctl_lock = 0xff; /* lock status hardware control port */

/*
 *	This array contains function pointers for every input
 *	I/O port (0 - 255), to do the required I/O.
 */
in_func_t *const port_in[256] = {
	[  0] = sios_in,	/* SIO status */
	[  1] = siod_in,	/* SIO data */
	[  4] = fdc_in,		/* FDC status */
	[ 64] = mmu_in,		/* MMU */
	[ 65] = clkc_in,	/* RTC read clock command */
	[ 66] = clkd_in,	/* RTC read clock data */
	[160] = hwctl_in,	/* virtual hardware control */
	[254] = fpsw_in,	/* mirror of port 255 */
	[255] = fpsw_in		/* read from front panel switches */
};

/*
 *	This array contains function pointers for every output
 *	I/O port (0 - 255), to do the required I/O.
 */
out_func_t *const port_out[256] = {
	[  0] = led_out,	/* blue LED */
	[  1] = siod_out,	/* SIO data */
	[  4] = fdc_out,	/* FDC command */
	[ 64] = mmu_out,	/* MMU */
	[ 65] = clkc_out,	/* RTC write clock command */
	[ 66] = clkd_out,	/* RTC write clock data */
	[160] = hwctl_out,	/* virtual hardware control */
	[254] = fpsw_out,	/* write to front panel switches */
	[255] = fpled_out	/* write to front panel lights (dummy) */
};

/*
 *	This function is to initiate the I/O devices.
 *	It will be called from the CPU simulation before
 *	any operation with the CPU is possible.
 */
void init_io(void)
{
}

/*
 *	This function is to stop the I/O devices. It is
 *	called from the CPU simulation on exit.
 */
void exit_io(void)
{
}

/*
 *	I/O handler for read SIO status:
 *	bit 0 = 0, character available for input from tty
 *	bit 7 = 0, transmitter ready to write character to tty
 */
static BYTE sios_in(void)
{
	register BYTE stat = 0b00000001; /* initially only output ready */
	size_t size;

	/* check if there is input from UART */
	if ((uart_get_buffered_data_len(CONFIG_ESP_CONSOLE_UART_NUM,
					&size) == ESP_OK) && (size > 0))
		stat &= 0b11111110;	/* if so flip status bit */

	return stat;
}

/*
 *	I/O handler for read SIO data.
 */
static BYTE siod_in(void)
{
	size_t size;

	if ((uart_get_buffered_data_len(CONFIG_ESP_CONSOLE_UART_NUM,
					&size) == ESP_OK) && (size > 0))
		sio_last = getchar();

	return sio_last;
}

/*
 *	read MMU register
 *	returns maximum bank in upper nibble
 *	and currently selected bank in lower nibble
 */
static BYTE mmu_in(void)
{
	return (NUMSEG << 4) | selbnk;
}

/*
 *	Input from virtual hardware control port
 *	returns lock status of the port
 */
static BYTE hwctl_in(void)
{
	return hwctl_lock;
}


/*
 *	Read virtual front panel switches state
 */
static BYTE fpsw_in(void)
{
	return fp_value;
}

/*
 *	Switch blue LED on/off.
 */
static void led_out(BYTE data)
{
	if (!data) {
		/* 0 switches LED off */
		gpio_set_level(LED_BLUE_PIN, 1);
	} else {
		/* everything else on */
		gpio_set_level(LED_BLUE_PIN, 0);
	}
}

/*
 *	Write byte to console.
 */
static void siod_out(BYTE data)
{
	putchar((int) data & 0x7f); /* strip parity, some software won't */
}

/*
 *	write MMU register
 */
static void mmu_out(BYTE data)
{
	if (selbnk > NUMSEG) {
		ESP_LOGE(TAG, "%04x: trying to select non-existing bank %d",
			 PC, data);
		cpu_error = IOERROR;
		cpu_state = ST_STOPPED;
		return;
	}
	selbnk = data;
	if (selbnk != 0)
		curbnk = bnks[selbnk - 1];
}

/*
 *	Port is locked until magic number 0xaa is received!
 *
 *	Virtual hardware control output.
 *	Used to shutdown and switch CPU's.
 *
 *	bit 4 = 1	switch CPU model to 8080
 *	bit 5 = 1	switch CPU model to Z80
 *	bit 6 = 1	reset system
 *	bit 7 = 1	halt emulation via I/O
 */
static void hwctl_out(BYTE data)
{
	/* if port is locked do nothing */
	if (hwctl_lock && (data != 0xaa))
		return;

	/* unlock port ? */
	if (hwctl_lock && (data == 0xaa)) {
		hwctl_lock = 0;
		return;
	}

	/* process output to unlocked port */
	/* but first lock port again */
	hwctl_lock = 0xff;

	if (data & 128) {
		cpu_error = IOHALT;
		cpu_state = ST_STOPPED;
		return;
	}

	if (data & 64) {
		reset_cpu();		/* reset CPU */
		reset_memory();		/* reset memory */
		PC = 0xff00;		/* power on jump to boot ROM */
		return;
	}

#if !defined (EXCLUDE_I8080) && !defined(EXCLUDE_Z80)
	if (data & 32) {		/* switch cpu model to Z80 */
		switch_cpu(Z80);
		return;
	}

	if (data & 16) {		/* switch cpu model to 8080 */
		switch_cpu(I8080);
		return;
	}
#endif
}

/*
 *	This allows to set the virtual front panel switches with ICE p command
 */
static void fpsw_out(BYTE data)
{
	fp_value = data;
}

/*
 *	Write output to front panel lights (dummy)
 */
static void fpled_out(BYTE data)
{
	UNUSED(data);
}
