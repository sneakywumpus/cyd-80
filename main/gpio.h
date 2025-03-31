/*
 * Z80SIM  -  a Z80-CPU simulator
 *
 * Copyright (C) 2025 by Thomas Eberhardt
 */

#ifndef GPIO_INC
#define GPIO_INC

/* RGB LED pins */
#define LED_RED_PIN		4
#define LED_GREEN_PIN		16
#define LED_BLUE_PIN		17

/* SD card slot SPI pins */
#define SDCARD_MISO_PIN		19
#define SDCARD_MOSI_PIN		23
#define SDCARD_CLK_PIN		18
#define SDCARD_CS_PIN		5

/* LCD SPI, data/command, and backlight pins */
#define LCD_MISO_PIN		12
#define LCD_MOSI_PIN		13
#define LCD_CLK_PIN		14
#define LCD_CS_PIN		15
#define LCD_DC_PIN		2
#define LCD_BL_PIN		21

/* XPT2046 touch screen SPI and IRQ pins */
#define TOUCH_MISO_PIN		39
#define TOUCH_MOSI_PIN		32
#define TOUCH_CLK_PIN		25
#define TOUCH_CS_PIN		33
#define TOUCH_IRQ_PIN		36

/* light sensor pin */
#define LDR_PIN			34

/* audio out pin */
#define AUDIO_PIN		26

#endif /* !GPIO_INC */
