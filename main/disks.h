/*
 * Z80SIM  -  a Z80-CPU simulator
 *
 * Copyright (C) 2024 by Udo Munk & Thomas Eberhardt
 *
 * This module implements the disks drives and low level
 * access functions for MicroSD, needed by the FDC.
 *
 * History:
 * 29-JUN-2024 split of from memsim.c and picosim.c
 */

#ifndef DISKS_INC
#define DISKS_INC

#include <stdio.h>

#include "sim.h"
#include "simdefs.h"

#define SD_MNTDIR "/sdcard"

#define NUMDISK	4	/* number of disk drives */
#define DISKLEN	29	/* path length for disk drives */
			/* /sdcard/DISKS80/filename.DSK */

extern int sd_file;
extern char disks[NUMDISK][DISKLEN];

extern void init_disks(void), exit_disks(void);
extern void list_files(const char *dir, const char *ext);
extern bool load_file(const char *name);
extern void check_disks(void);
extern void mount_disk(int drive, const char *name);

extern BYTE read_sec(int drive, int track, int sector, WORD addr);
extern BYTE write_sec(int drive, int track, int sector, WORD addr);
extern void get_fdccmd(BYTE *cmd, WORD addr);

#endif /* !DISK_INC */
