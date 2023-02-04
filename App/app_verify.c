#include "DoomPlayer.h"
#include "board_if.h"
#include "fatfs.h"
#include <stdlib.h>
#include "app_task.h"

#define	MAX_WADS	10		// Number of WAD files we can support

typedef struct {
  char flmagic[8];
  WADPROP *pInfo;
} BOOT_INFO;

SECTION_SRDSRAM BOOT_INFO BootInfo;

#define	MAGIC_CHECKx

typedef struct {
  char      *fname;
  int       checked;
  uint32_t  fsize;
  uint32_t  foffset;
} CHECK_LIST;

/*
 * List of files we are going to check its existence on the SD card.
 */
static CHECK_LIST FileList[] = {
  { "default.cfg",     0 },
  { "disco-doom.cfg", 0 },
  { "doom1-music.cfg", 0 },
  { "doom2-music.cfg", 0 },
  { "tnt-music.cfg",   0 },
  { "Doom1.bin", 0 },
  { "DoomUlt.bin", 0 },
  { "DoomII1.bin", 0 },
  { "DoomII2.bin", 0 },
  { "Doom3.bin", 0 },
  { "Doom3a.bin", 0 },
  { "Doom3RE1.bin", 0 },
  { "Doom3RE2.bin", 0 },
  { "dualsense.ibin", 0 },
  { "tnt-music",    0 },
  { "doom1-music",    0 },
  { "doom2-music",    0 },
};

/*
 * Name of Music configuration files.
 */
static const char *mconfig_files[3] = {
    "doom1-music.cfg",
    "doom2-music.cfg",
    "tnt-music.cfg",
};

/*
 * List of official IWAD files we can support
 *  Length and CRC values are found at https://doomwiki.org/wiki/IWAD
 */
const WADPROP ValidWads[] = {
 { 11159840, 0x723e60f9, "DOOM.WAD",     "DOOM Registered" },			// DOOM.WAD  Version 1.9
 { 12408292, 0xbf0eaac0, "DOOM.WAD",     "The Ultimate DOOM" },			// DOOM.WAD  Version 1.9ud
 {  4196020, 0x162b696a, "DOOM1.WAD",    "DOOM Shareware" },			// DOOM1.WAD Version 1.9
 { 14604584, 0xec8725db, "DOOM2.WAD",    "DOOM 2: Hell on Earth" },		// DOOM2.WAD Version 1.9
 { 17420824, 0x48d1453c, "PLUTONIA.WAD", "Final Doom: Plutonia Experiment" },	// PLUTONIA.WAD Version 1.9
 { 18195736, 0x903dcc27, "TNT.WAD",      "Final Doom: TNT - Evilution" },	// TNT.WAD   Version 1.9
 { 18654796, 0xd4bb05c0, "TNT.WAD",      "Final Doom: TNT - Evilution" },	// TNT.WAD  id Anthology
 {        0,          0, NULL },
};

static WADLIST *WadList;

extern CRC_HandleTypeDef hcrc;

static const char *findWadName(char *fname, int fsize)
{
  const WADPROP *wadp = ValidWads;

  while (wadp->length)
  {
    if (wadp->length == fsize)
     return wadp->wadname;
    wadp++;
  }
  return fname;
}

static const WADPROP *ValidateDoomWAD(FILINFO *finfo)
{
  const WADPROP *pInfo;
  int fsize;


  if (finfo->fname[0] == '.')
     return NULL;

  fsize = finfo->fsize;
  for (pInfo = ValidWads; pInfo->length; pInfo++)
  {
    if (fsize == pInfo->length)
    {
      return pInfo;
    }
  }
  return NULL;
}

/**
 * @brief Verify contents of SD card.
 */
int VerifySDCard(void **errString1, void **errString2)
{
  FRESULT res;
  DIR DirInfo;
  static FILINFO fno;
  int lsize, nfound;
  CHECK_LIST *clist;
  int i;
  uint32_t foffset;

  lsize = sizeof(FileList)/sizeof(CHECK_LIST);
  res = f_opendir(&DirInfo, GAME_DIR);
  if (res)
  {
    *errString1 = "Can't open";
    *errString2 = "directory.";
    return -1;
  }

  /* Scan /GameData directory. */

  foffset = sizeof(QSPI_DIRHEADER) + 255;
  foffset &= ~255;

  for (;;)
  {
    res = f_readdir(&DirInfo, &fno);
    if (res != FR_OK || fno.fname[0] == 0) break;

    clist = FileList;
    for (i = 0; i < lsize; i++)
    {
      if (strncmp(fno.fname, clist->fname, strlen(clist->fname)) == 0)
      {
        clist->checked = 1;
        clist->fsize = fno.fsize;
        clist->foffset = foffset;
        if ((fno.fattrib & AM_DIR) == 0)
        {
          foffset += clist->fsize;
        }
        break;
      }
      clist++;
    }
    foffset += 255;	// Allocate on page boundary
    foffset &= ~255;
  }
  f_closedir(&DirInfo);

  clist = FileList;
  nfound = 0;

  for (i = 0; i < lsize; i++)
  {
    if (clist->checked == 0)
    {
       debug_printf("%s not found.\n", clist->fname);
       *errString1 = clist->fname;
       *errString2 = "not found.";
       return -2;
    }
    nfound++;
    clist++;
  }

  /* Scan WAD files in Games directory */

  res = f_chdir(GAME_DIR);
  if (res == FR_OK)
    res = f_findfirst(&DirInfo, &fno, "", "*.WAD");
  if (res != FR_OK)
  {
    *errString1 = "Can't open";
    *errString2 = "Games directory.";
    f_chdir("/");
    return -1;
  }

  WadList = (WADLIST *)malloc(sizeof(WADLIST) * MAX_WADS);
  memset(WadList, 0, sizeof(WADLIST) * MAX_WADS);

  i = 0;
  while ((i < MAX_WADS) && res == FR_OK && fno.fname[0])
  {
    WadList[i].wadInfo = ValidateDoomWAD(&fno);
    if (WadList[i].wadInfo)
    {
      memcpy(WadList[i].fname, fno.fname, strlen(fno.fname));
      WadList[i].fsize = fno.fsize;
      i++;
    }
    res = f_findnext(&DirInfo, &fno);
  }

  f_closedir(&DirInfo);

  f_chdir("/");

#ifdef DEBUG_VERIFY
  WADLIST *list;

  for (list = WadList; list->wadInfo; list++)
  {
    debug_printf("%s: %s\n", list->fname, list->wadInfo->title);
  }
#endif

  *errString1 = WadList;
  return foffset;
}

/*
 *  Verify IWAD file on the SPI Flash.
 */
int VerifyFlash(void **p1, void **p2)
{
  QSPI_DIRHEADER *dhp;
  FS_DIRENT *fsp;
  uint32_t crcval;
  const WADPROP *pInfo;
  int flash_size;

  flash_size = Board_FlashInfo();
  if (flash_size < 0)
  {
    debug_printf("OSPI_NOR_GetInfo failed.\n");
    *p1 = "Faild to get";
    *p2 = "Flash Information.";
    return -1;
  }
  debug_printf("%d KB flash found.\n", flash_size / 1024);
  Board_Flash_Init();
  osDelay(10);	// Insert little delay to make sure QSPI is really mapped.

  dhp = (QSPI_DIRHEADER *)QSPI_ADDR;

  if (dhp->fs_magic != DOOMFS_MAGIC)
  {
    *p1 = "Flash not initialized";
    *p2 = "Need to copy from SD card.";
    return -2;
  }

#ifdef MAGIC_CHECK
  if (memcmp(BootInfo.flmagic, "FL_VERIFY", 8) == 0)
  {
    pInfo = BootInfo.pInfo;
    *p1 = (WADPROP *)pInfo;
    *p2 = pInfo->title;
    return 0;
  }
#endif

  fsp = dhp->fs_direntry;

  crcval = HAL_CRC_Calculate(&hcrc, (uint32_t *)(QSPI_ADDR + fsp->foffset), fsp->fsize);
  crcval = ~crcval;

  for (pInfo = ValidWads; pInfo->length; pInfo++)
  {
    if (crcval == pInfo->crcval)
    {
      debug_printf("Flash: %s\n", pInfo->title);
      *p1 = (WADPROP *)pInfo;
      *p2 = (void *)pInfo->title;
#ifdef MAGIC_CHECK
      memcpy(BootInfo.flmagic, "FL_VERIFY", 8);
      BootInfo.pInfo = (void *)pInfo;
      osDelay(100);
#endif
      return 0;
    }
  }
  debug_printf("crcval = %d, %x\n", crcval, crcval);
  *p1 = "No Game image";
  *p2 = "found.";

  return -1;
}

#define	WBSIZE	2048
static SECTION_AXISRAM uint8_t copybuffer[WBSIZE];

#define DO_ERASE
#define	DO_WRITE

/*
 * Copy file from SD card to SPI flash
 */
static int copyfile(char *fname, uint32_t fsize, uint32_t foffset)
{
  int esize, wsize, rsize;
  UINT nb;
  uint32_t baddr;
  uint8_t *bp;
  FIL *cFile;
  GUI_EVENT guiev;
  int bcount;

  guiev.evcode = GUIEV_COPY_REPORT;
  guiev.evval0 = OP_START;
  guiev.evarg1 = fname;
  guiev.evarg2 = NULL;
  postGuiEvent(&guiev);
  osDelay(20);

  baddr = foffset;
  rsize = fsize;
  bp = copybuffer;

  cFile = OpenFATFile(fname);

  if (cFile == NULL)
  {
    debug_printf("%s: Not found.\n", fname);
    f_chdir("/");
    guiev.evval0 = OP_ERROR;
    guiev.evarg1 = fname;
    guiev.evarg2 = "not found.";
    postGuiEvent(&guiev);
    return -1;
  }

  bcount = 0;

  for (esize = 0; esize < fsize; esize += WBSIZE)
  {
    wsize = (rsize > WBSIZE)? WBSIZE : rsize;
    if (f_read(cFile, bp, wsize, &nb) != FR_OK)
    {
      guiev.evval0 = OP_ERROR;
      guiev.evarg1 = "SD read";
      guiev.evarg2 = "error.";
      postGuiEvent(&guiev);
      break;;
    }
    rsize -= nb;
    if (bcount == 0)
    {
      guiev.evval0 = OP_PROGRESS;
      guiev.evarg1 = (void *)(esize * 100 / fsize);
      postGuiEvent(&guiev);
    }
    bcount++;
    bcount &= 7;
#ifdef DO_WRITE
    if (Board_Flash_Write(bp, baddr, wsize) != 0)
    {
      debug_printf("Write failed at %x\n", baddr);
      guiev.evval0 = OP_ERROR;
      guiev.evarg1 = "Flash write";
      guiev.evarg2 = "error.";
      postGuiEvent(&guiev);
      break;;
    }
    osDelay(2);
#endif
    baddr += wsize;
  }
  CloseFATFile(cFile);

  return 0;
}

/*
 * Copy specified IWAD and necessary files from SD to SPI flash
 */
void CopyFlash(WADLIST *list, uint32_t foffset)
{
  int fsize, esize;
  int baddr;
  GUI_EVENT guiev;
  int block_size;
  int i;

  f_chdir(GAME_DIR);

  guiev.evcode = GUIEV_ERASE_REPORT;

  Board_Flash_ReInit(0);
  block_size = Board_EraseSectorSize();
    
  /* fsize shows total number of required flash space. */
  fsize = sizeof(QSPI_DIRHEADER) + foffset + list->fsize;

  guiev.evval0 = OP_START;
  guiev.evarg1 = NULL;
  guiev.evarg2 = NULL;
  postGuiEvent(&guiev);

  baddr = 0;;

  /* Erase required flash space. */

  debug_printf("Erasing..\n");
  for (esize = 0; esize < fsize; esize += block_size)
  {
#ifdef DO_ERASE
    Board_Erase_Block(baddr);
#endif
    baddr += block_size;

    guiev.evval0 = OP_PROGRESS;
    guiev.evarg1 = (void *)(esize * 100 / fsize);
    postGuiEvent(&guiev);
  }
  debug_printf("Erase done.\n");
  osDelay(10);

  guiev.evcode = GUIEV_COPY_REPORT;
  guiev.evval0 = OP_START;
  guiev.evarg1 = NULL;
  guiev.evarg2 = NULL;
  postGuiEvent(&guiev);
  osThreadYield();

  int lsize;
  CHECK_LIST *clist;
  QSPI_DIRHEADER *dirhp;
  FS_DIRENT *dp;

  dirhp = malloc(sizeof(QSPI_DIRHEADER));
  memset(dirhp, 0, sizeof(QSPI_DIRHEADER));

  /* Create QSPI_DIRHEADER information */

  dirhp->fs_magic = DOOMFS_MAGIC;
  dirhp->fs_fcount = NUM_DIRENT;
  dirhp->fs_version = 1;
  dirhp->fs_size = 0;
  dp = dirhp->fs_direntry;

  lsize = sizeof(FileList)/sizeof(CHECK_LIST);
  clist = FileList;

  /* Set WAD file info at the top. */

#if 0
  memcpy(dp->fname, list->fname, strlen(list->fname));
#else
  const char *sp;

  sp = findWadName(list->fname, list->fsize);
  memcpy(dp->fname, sp, strlen(sp));
#endif
  dp->fsize = list->fsize;
  dp->foffset = foffset;
  dp++;

  /* Add info for other files. */

  for (i = 0; i < lsize; i++)
  {
    if (clist->fsize)
    {
      memcpy(dp->fname, clist->fname, strlen(clist->fname) + 1);
      dp->fsize = clist->fsize;
      dp->foffset = clist->foffset;
      dp++;
    }
    clist++;
  }
#ifdef DO_WRITE
  /* Write QSPI_DIRHEADER part */

  if (Board_Flash_Write((uint8_t *)dirhp, 0, sizeof(QSPI_DIRHEADER)) != 0)
  {
    guiev.evval0 = OP_ERROR;
    guiev.evarg1 = "Flash write";
    guiev.evarg2 = "error.";
    postGuiEvent(&guiev);

    Board_Flash_ReInit(1);
    return;
  }
#endif

  /* Copy config files and Image files. */
  clist = FileList;
  for (i = 0; i < lsize; i++)
  {
    if (clist->fsize)
    {
      copyfile(clist->fname, clist->fsize, clist->foffset);
    }
    if (guiev.evval0 == OP_ERROR)
      break;
    clist++;
  }

  /* Copy WAD file */
  copyfile(list->fname, list->fsize, foffset);

  free(dirhp);

  debug_printf("Write done.\n");

  if (guiev.evval0 != OP_ERROR)
  {
    guiev.evval0 = OP_DONE;
    guiev.evarg1 = NULL;
    guiev.evarg2 = NULL;
  }
  postGuiEvent(&guiev);

  Board_Flash_ReInit(1);
  f_chdir("/");
}

void LoadMusicConfigs()
{
  int i;
  char *path;

  for (i = 0; i < 3; ++i)
  {
     path = (char *)mconfig_files[i];
     ReadMusicList(path);
  }
}
