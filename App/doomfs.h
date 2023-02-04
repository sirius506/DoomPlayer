#ifndef __DOOMFS_H__
#define __DOOMFS_H__
#include "targetdev_conf.h"

/*
 * DOOM file system is build from the files under GAME_DIR of SD card.
 * All necessary config, music dta and WAD files should be located under this
 * directory.
 */
#define	GAME_DIR	"/GameData"

/* Our DOOM file system structure definitions */

#define	DOOMFS_MAGIC	0x444D4653
#define	NUM_DIRENT	20
#define	FS_NAMELEN	16

/* At the top of the SPI flash, we have QSPI_DIRHEADER structure */

typedef struct {
  char     fname[FS_NAMELEN];	// file name
  uint32_t fsize;		// file size
  uint32_t foffset;		// file location offset
} FS_DIRENT;

typedef struct s_qspiDir {
  uint32_t fs_magic;		// Should be DOOMFS_MAGIC
  uint32_t fs_fcount;		// should be NUM_DIRENT
  uint32_t fs_version;		// place holder
  uint32_t fs_size;
  FS_DIRENT fs_direntry[NUM_DIRENT];
} QSPI_DIRHEADER;

#endif
