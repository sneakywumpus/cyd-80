/*
 * Z80SIM  -  a Z80-CPU simulator
 *
 * Copyright (C) 2024 by Udo Munk & Thomas Eberhardt
 *
 * This module configures the machine appropriate for the
 * Z80/8080 software we want to run on it.
 *
 * History:
 * 20-APR-2024 dummy, no configuration implemented yet
 * 12-MAY-2024 implemented configuration dialog
 * 27-MAY-2024 implemented load file
 * 28-MAY-2024 implemented mount/unmount of disk images
 * 03-JUN-2024 added directory list for code files and disk images
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>

#include "sim.h"
#include "simdefs.h"
#include "simglb.h"
#include "simcore.h"
#include "simport.h"
#include "simio.h"
#include "simcfg.h"

#include "disks.h"
#include "cydsim.h"

/*
 * prompt for a filename
 */
static void prompt_fn(char *s, const char *ext)
{
	printf("Filename (without .%s): ", ext);
	get_cmdline(s, 9);
	while (*s) {
		*s = toupper((unsigned char) *s);
		s++;
	}
}

/*
 * get an integer with range check
 */
static int get_int(const char *prompt, const char *hint,
		   int min_val, int max_val)
{
	int i;
	char s[6];

	for (;;) {
		printf("Enter %s%s: ", prompt, hint);
		get_cmdline(s, 5);
		if (s[0] == '\0')
			return -1;
		i = atoi(s);
		if (i < min_val || i > max_val) {
			printf("Invalid %s: range %d - %d\n",
			       prompt, min_val, max_val);
		} else
			return i;
	}
}

/*
 * Configuration dialog for the machine
 */
void config(void)
{
	const char *cfg = SD_MNTDIR "/CONF80/CYD80.DAT";
	const char *cpath = SD_MNTDIR "/CODE80";
	const char *cext = "*.BIN";
	const char *dpath = SD_MNTDIR "/DISKS80";
	const char *dext = "*.DSK";
	char s[10];
	unsigned int br;
	int go_flag = 0;
	int i, menu;

	/* try to read config file */
	sd_file = open(cfg, O_RDONLY);
	if (sd_file >= 0) {
		br = read(sd_file, &cpu, sizeof(cpu));
		br = read(sd_file, &speed, sizeof(speed));
		br = read(sd_file, &fp_value, sizeof(fp_value));
		br = read(sd_file, &disks[0], DISKLEN);
		br = read(sd_file, &disks[1], DISKLEN);
		br = read(sd_file, &disks[2], DISKLEN);
		br = read(sd_file, &disks[3], DISKLEN);
		(void) br;
		close(sd_file);
	}
	menu = 1;

	while (!go_flag) {
		if (menu) {
			printf("c - switch CPU, currently %s\n",
			       (cpu == Z80) ? "Z80" : "8080");
			printf("s - CPU speed: ");
			if (speed == 0)
				puts("unlimited");
			else
				printf("%d MHz\n", speed);
			printf("p - Port 255 value: %02XH\n", fp_value);
			printf("f - list files\n");
			printf("r - load file\n");
			printf("d - list disks\n");
			printf("0 - Disk 0: %s\n", disks[0]);
			printf("1 - Disk 1: %s\n", disks[1]);
			printf("2 - Disk 2: %s\n", disks[2]);
			printf("3 - Disk 3: %s\n", disks[3]);
			printf("g - run machine\n\n");
		} else
			menu = 1;
		printf("Command: ");
		get_cmdline(s, 2);
		putchar('\n');

		switch (tolower((unsigned char) s[0])) {
		case 'c':
			if (cpu == Z80)
				switch_cpu(I8080);
			else
				switch_cpu(Z80);
			break;

		case 's':
			i = get_int("speed", " in MHz (0=unlimited)", 0, 40);
			putchar('\n');
			if (i >= 0)
				speed = i;
			break;

		case 'p':
again:
			printf("Enter value in Hex: ");
			get_cmdline(s, 3);
			if (s[0]) {
				if (!isxdigit((unsigned char) s[0]) ||
				    !isxdigit((unsigned char) s[1])) {
					puts("Invalid value: range 00 - FF");
					goto again;
				}
				fp_value = (s[0] <= '9' ? s[0] - '0' :
					    toupper((unsigned char) s[0]) -
					    'A' + 10) << 4;
				fp_value += (s[1] <= '9' ? s[1] - '0' :
					     toupper((unsigned char) s[1]) -
					     'A' + 10);
			}
			putchar('\n');
			break;

		case 'f':
			list_files(cpath, cext);
			putchar('\n');
			menu = 0;
			break;

		case 'r':
			prompt_fn(s, "BIN");
			if (s[0])
				load_file(s);
			putchar('\n');
			menu = 0;
			break;

		case 'd':
			list_files(dpath, dext);
			putchar('\n');
			menu = 0;
			break;

		case '0':
		case '1':
		case '2':
		case '3':
			i = s[0] - '0';
			prompt_fn(s, "DSK");
			if (s[0])
				mount_disk(i, s);
			else {
				disks[i][0] = '\0';
				putchar('\n');
			}
			break;

		case 'g':
			go_flag = 1;
			break;

		default:
			break;
		}
	}

	/* try to save config file */
	sd_file = open(cfg, O_WRONLY | O_CREAT, 0666);
	if (sd_file >= 0) {
		br = write(sd_file, &cpu, sizeof(cpu));
		br = write(sd_file, &speed, sizeof(speed));
		br = write(sd_file, &fp_value, sizeof(fp_value));
		br = write(sd_file, &disks[0], DISKLEN);
		br = write(sd_file, &disks[1], DISKLEN);
		br = write(sd_file, &disks[2], DISKLEN);
		br = write(sd_file, &disks[3], DISKLEN);
		(void) br;
		close(sd_file);
	}
}
