/*
 * Z80SIM  -  a Z80-CPU simulator
 *
 * Copyright (C) 2024 Thomas Eberhardt
 */

#ifndef SIMPORT_INC
#define SIMPORT_INC

#include <stdint.h>
#include <unistd.h>

#include "esp_timer.h"

static inline void sleep_for_us(long time) { usleep(time); }
static inline void sleep_for_ms(int time) { usleep(time * 1000L); }

static inline uint64_t get_clock_us(void)
{
	return esp_timer_get_time();
}

extern int get_cmdline(char *buf, int len);

#endif /* !SIMPORT_INC */
