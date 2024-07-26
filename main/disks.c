/*
 * Z80SIM  -  a Z80-CPU simulator
 *
 * Copyright (C) 2024 by Udo Munk & Thomas Eberhardt
 *
 * This module implements the disks drives and low level
 * access functions for MicroSD, needed by the FDC.
 *
 * History:
 * 27-MAY-2024 implemented load file
 * 28-MAY-2024 implemented sector I/O to disk images
 * 03-JUN-2024 added directory list for code files and disk images
 * 29-JUN-2024 split of from memsim.c and picosim.c
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

#include "sim.h"
#include "simdefs.h"
#include "simglb.h"
#include "simmem.h"

#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

#include "sd-fdc.h"
#include "disks.h"
#include "cydsim.h"

static const char *TAG = "disks";

int sd_file;	/* at any time we have only one file open */
char disks[NUMDISK][DISKLEN]; /* path name for 4 disk images
				 /sdcard/DISKS80/filename.DSK */

static sdmmc_host_t host = SDSPI_HOST_DEFAULT(); /* default 20MHz */
static sdmmc_card_t *card;

/* buffer for disk/memory transfers */
static unsigned char dsk_buf[SEC_SZ];

void init_disks(void)
{
	esp_err_t ret;
	spi_bus_config_t bus_cfg = {
		.mosi_io_num = SDCARD_MOSI_PIN,
		.miso_io_num = SDCARD_MISO_PIN,
		.sclk_io_num = SDCARD_CLK_PIN,
		.quadwp_io_num = -1,
		.quadhd_io_num = -1,
		.max_transfer_sz = 4000
	};
	sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
	esp_vfs_fat_sdmmc_mount_config_t mount_config = {
		.format_if_mount_failed = false,
		.max_files = 1,
		.allocation_unit_size = 16 * 1024
	};

	ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to initialize bus.");
		abort();
	}

	slot_config.gpio_cs = SDCARD_CS_PIN;
	slot_config.host_id = host.slot;
	ret = esp_vfs_fat_sdspi_mount(SD_MNTDIR, &host, &slot_config,
				      &mount_config, &card);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to mount filesystem.");
		abort();
	}
}

void exit_disks(void)
{
	/* unmount SD card */
	esp_vfs_fat_sdcard_unmount(SD_MNTDIR, card);

	/* deinitialize the bus after all devices are removed */
	spi_bus_free(host.slot);
}

/*
 * list files with pattern 'ext' in directory 'dir'
 * (ext is currently ignored)
 */
void list_files(const char *dir, const char *ext)
{
	DIR *dp;
	struct dirent *entp;
	register int i = 0;

	dp = opendir(dir);
	if (dp != NULL) {
		while ((entp = readdir(dp)) != NULL) {
			printf("%s\t", entp->d_name);
			if (strlen(entp->d_name) < 8)
				putchar('\t');
			i++;
			if (i > 4) {
				putchar('\n');
				i = 0;
			}
		}
		if (i > 0)
			putchar('\n');
	}
}

/*
 * load a file 'name' into memory
 */
void load_file(const char *name)
{
	int i = 0;
	register unsigned int j;
	ssize_t br;
	char SFN[30];

	strcpy(SFN, SD_MNTDIR);
	strcat(SFN, "/CODE80/");
	strcat(SFN, name);
	strcat(SFN, ".BIN");

	/* try to open file */
	sd_file = open(SFN, O_RDONLY);
	if (sd_file < 0) {
		puts("File not found");
		return;
	}

	/* read file into memory */
	while ((br = read(sd_file, &dsk_buf[0], SEC_SZ)) > 0) {
		for (j = 0; j < br; j++)
			dma_write(i + j, dsk_buf[j]);
		if (br < SEC_SZ)	/* last record reached */
			break;
		i += SEC_SZ;
	}
	if (br < 0)
		printf("fread error: %s (%d)\n", strerror(errno), errno);
	else
		printf("loaded file \"%s\" (%d bytes)\n", SFN, i + br);

	close(sd_file);
}

/*
 * check that all disks refer to existing files
 */
void check_disks(void)
{
	int i, n = 0;

	for (i = 0; i < NUMDISK; i++) {
		if (disks[i][0]) {
			/* try to open file */
			sd_file = open(disks[i], O_RDONLY);
			if (sd_file < 0) {
				printf("Disk image \"%s\" no longer exists.\n",
				       disks[i]);
				disks[i][0] = '\0';
				n++;
			} else
				close(sd_file);
		}
	}
	if (n > 0)
		putchar('\n');
}

/*
 * mount a disk image 'name' on disk 'drive'
 */
void mount_disk(int drive, const char *name)
{
	char SFN[DISKLEN];
	int i;

	strcpy(SFN, SD_MNTDIR);
	strcat(SFN, "/DISKS80/");
	strcat(SFN, name);
	strcat(SFN, ".DSK");

	for (i = 0; i < NUMDISK; i++) {
		if (i != drive && strcmp(disks[i], SFN) == 0) {
			puts("Disk already mounted\n");
			return;
		}
	}

	/* try to open file */
	sd_file = open(SFN, O_RDONLY);
	if (sd_file < 0) {
		puts("File not found\n");
		return;
	}

	close(sd_file);
	strcpy(disks[drive], SFN);
	putchar('\n');
}

/*
 * prepare I/O for sector read and write routines
 */
static BYTE prep_io(int drive, int track, int sector, WORD addr)
{
	off_t pos;

	/* check if drive in range */
	if ((drive < 0) || (drive > 3))
		return FDC_STAT_DISK;

	/* check if track and sector in range */
	if (track > TRK)
		return FDC_STAT_TRACK;
	if ((sector < 1) || (sector > SPT))
		return FDC_STAT_SEC;

	/* check if DMA address in range */
	if (addr > 0xff7f)
		return FDC_STAT_DMAADR;

	/* check if disk in drive */
	if (!disks[drive][0])
		return FDC_STAT_NODISK;

	/* open file with the disk image */
	sd_file = open(disks[drive], O_RDWR);
	if (sd_file < 0)
		return FDC_STAT_NODISK;

	/* seek to track/sector */
	pos = (((off_t) track * (off_t) SPT) + sector - 1) * SEC_SZ;
	if (lseek(sd_file, pos, SEEK_SET) < 0) {
		close(sd_file);
		return FDC_STAT_SEEK;
	}
	return FDC_STAT_OK;
}

/*
 * read from drive a sector on track into memory @ addr
 */
BYTE read_sec(int drive, int track, int sector, WORD addr)
{
	BYTE stat;
	ssize_t br;
	register int i;

	/* turn on green LED */
	gpio_set_level(LED_GREEN_PIN, 0);

	/* prepare for sector read */
	if ((stat = prep_io(drive, track, sector, addr)) == FDC_STAT_OK) {

		/* read sector into memory */
		br = read(sd_file, &dsk_buf[0], SEC_SZ);
		if (br >= 0) {
			if (br < SEC_SZ)	/* UH OH */
				stat = FDC_STAT_READ;
			else {
				for (i = 0; i < SEC_SZ; i++)
					dma_write(addr + i, dsk_buf[i]);
				stat = FDC_STAT_OK;
			}
		} else
			stat = FDC_STAT_READ;

		close(sd_file);
	}

	/* turn off green LED */
	gpio_set_level(LED_GREEN_PIN, 1);

	return stat;
}

/*
 * write to drive a sector on track from memory @ addr
 */
BYTE write_sec(int drive, int track, int sector, WORD addr)
{
	BYTE stat;
	ssize_t br;
	register int i;

	/* turn on red LED */
	gpio_set_level(LED_RED_PIN, 0);

	/* prepare for sector write */
	if ((stat = prep_io(drive, track, sector, addr)) == FDC_STAT_OK) {

		/* write sector to disk image */
		for (i = 0; i < SEC_SZ; i++)
			dsk_buf[i] = dma_read(addr + i);
		br = write(sd_file, &dsk_buf[0], SEC_SZ);
		if (br >= 0) {
			if (br < SEC_SZ)	/* UH OH */
				stat = FDC_STAT_WRITE;
			else
				stat = FDC_STAT_OK;
		} else
			stat = FDC_STAT_WRITE;

		close(sd_file);
	}

	/* turn off red LED */
	gpio_set_level(LED_RED_PIN, 1);

	return stat;
}

/*
 * get FDC command from CPU memory
 */
void get_fdccmd(BYTE *cmd, WORD addr)
{
	register int i;

	for (i = 0; i < 4; i++)
		cmd[i] = dma_read(addr + i);
}
