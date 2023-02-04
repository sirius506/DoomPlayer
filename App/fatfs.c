/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file   fatfs.c
  * @brief  Code for fatfs applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2022 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
#include "DoomPlayer.h"
#include "fatfs.h"
#include "doomtype.h"
/* USER CODE END Header */

/* USER CODE BEGIN Variables */
uint8_t retSD;    /* Return value for SD */
char SDPath[4];   /* SD logical drive path */

SECTION_AXISRAM FATFS SDFatFS;    /* File system object for SD logical drive */
SECTION_AXISRAM FIL SDFile;       /* File object for SD */
SECTION_AXISRAM FILINFO SDInfo;
SECTION_AXISRAM FIL FlacFile;
SECTION_AXISRAM FIL JpegFile;

SEMAPHORE_DEF(sem_sdfile)

osSemaphoreId_t sdfile_semId;

/* USER CODE END Variables */

void MX_FATFS_Init(void)
{
  /*## FatFS: Link the SD driver ###########################*/
  retSD = FATFS_LinkDriver(&SD_Driver, SDPath);

  /* USER CODE BEGIN Init */
  /* additional user code for init */
  retSD = f_mount(&SDFatFS, (TCHAR const*)SDPath, 0);

  sdfile_semId = osSemaphoreNew(1, 1, &attributes_sem_sdfile);
  /* USER CODE END Init */
}

/**
  * @brief  Gets Time from RTC
  * @param  None
  * @retval Time in DWORD
  */
DWORD get_fattime(void)
{
  /* USER CODE BEGIN get_fattime */
  return 0;
  /* USER CODE END get_fattime */
}

/* USER CODE BEGIN Application */

FIL *OpenFATFile(char *name)
{
  FRESULT res;

  osSemaphoreAcquire(sdfile_semId, osWaitForever);

  res = f_open(&SDFile, name, FA_READ);
  if (res != FR_OK)
  {
    osSemaphoreRelease(sdfile_semId);
    return NULL;
  }
  return &SDFile;
}

FIL *CreateFATFile(char *name)
{
  FRESULT res;

  osSemaphoreAcquire(sdfile_semId, osWaitForever);

  res = f_open(&SDFile, name, FA_CREATE_ALWAYS|FA_WRITE);
  if (res != FR_OK)
  {
    debug_printf("Create Failed: %d\n", res);
    osSemaphoreRelease(sdfile_semId);
    return NULL;
  }
  return &SDFile;
}

void CloseFATFile(FIL *pfile)
{
  f_close(pfile);
  osSemaphoreRelease(sdfile_semId);
}

FIL *OpenFlacFile(char *name)
{
  FRESULT res;

  res = f_open(&FlacFile, name, FA_READ);
  if (res != FR_OK)
    return NULL;
  return &FlacFile;
}

void CloseFlacFile(FIL *pfile)
{
  f_close(pfile);
}

FIL *CreateJpegFile(char *name)
{
  FRESULT res;

  res = f_open(&JpegFile, name, FA_CREATE_ALWAYS|FA_WRITE);
  if (res != FR_OK)
    return NULL;
  return &JpegFile;
}

void CloseJpegFile(FIL *pfile)
{
  f_close(pfile);
}

boolean M_SDFileExists(char *filename)
{
  FRESULT res;

  res = f_stat(filename, &SDInfo);
  if (res == FR_OK)
  {
    return true;
  }
  return false;
}

FRESULT M_SDMakeDirectory(char *dirname)
{
  FRESULT res;

  res = f_mkdir(dirname);
  return res;
}

/* USER CODE END Application */
