#include "DoomPlayer.h"
#include <stdio.h>
#include "fatfs.h"
#include "board_if.h"
#include "app_task.h"
#include "jpeg_if.h"

int __errno;

#define	USE_CONSOLE

TASK_DEF(lvgl,    2200, osPriorityBelowNormal2)
TASK_DEF(consTask, 400, osPriorityNormal);
//TASK_DEF(shotTask, 400, osPriorityLow3);
TASK_DEF(shotTask, 400, osPriorityNormal2);

#define REQCMD_DEPTH     6
static uint8_t reqcmdBuffer[REQCMD_DEPTH * sizeof(REQUEST_CMD)];

#define SHOTQ_DEPTH     6
static uint8_t shotqBuffer[SHOTQ_DEPTH * sizeof(SCREENSHOT_CMD)];

MESSAGEQ_DEF(reqcmdq, reqcmdBuffer, sizeof(reqcmdBuffer))
MESSAGEQ_DEF(shotq, shotqBuffer, sizeof(shotqBuffer))

static osMessageQueueId_t  reqcmdqId;
static osMessageQueueId_t  shotqId;

extern FRESULT M_SDMakeDirectory(char *dir);

const HeapRegion_t xHeapRegions[2] = {
  { (uint8_t *)RTOS_HEAP_ADDR, RTOS_HEAP_SIZE },
  { NULL, 0 }
};

void *malloc(size_t size)
{
  void *p;

  p = (void *)pvPortMalloc(size);
#ifdef VERBOSE_DEBUG
  if (size > 64 * 1024)
    debug_printf("%s: %d --> %x\n", __FUNCTION__, (int)size, p);
#endif
  if (p == NULL)
    debug_printf("%s: NO MEM %d --> %x\n", __FUNCTION__, (int)size, p);
  return p;
}

void *calloc(size_t count, size_t size)
{
  size_t amount = count * size;
  void *p;

  p = malloc(amount);
  if (p)
    memset(p, 0, amount);
  return p;
}

void free(void *ptr)
{
  vPortFree(ptr);
}

void *realloc(void *ptr, size_t size)
{
  void *p;

  if (ptr)
  {
    uint32_t *vp;
    int len;

    vp = (uint32_t *)((int32_t)ptr - 4);
    len = *vp & 0xffffff;
//debug_printf("realloc: vp = %x, %d -> %d\n", vp, len, size);
    p = malloc(size);
    if (p)
    {
      memcpy(p, ptr, len);
      free(ptr);
    }
  }
  else
  {
    p = malloc(size);
  }
  return p;
}

char *strdup(const char *s1)
{
  int slen = strlen(s1);
  char *dp;

  dp = pvPortMalloc(slen + 1);
  memcpy(dp, s1, slen + 1);
  return dp;
}

#define	FNAME_LEN	30

void StartDefaultTask(void *argv)
{
  char *errs1, *errs2;
  GUI_EVENT guiev;
  int res;
  int wait_time;

  reqcmdqId = osMessageQueueNew(REQCMD_DEPTH, sizeof(REQUEST_CMD), &attributes_reqcmdq);
  shotqId = osMessageQueueNew(SHOTQ_DEPTH, sizeof(REQUEST_CMD), &attributes_shotq);

  MX_FATFS_Init();

  Board_LCD_Init();

#ifdef USE_CONSOLE
  osThreadNew(StartConsoleTask, NULL, &attributes_consTask);
#endif

  osThreadNew(StartLvglTask, NULL, &attributes_lvgl);

  osThreadNew(StartShotTask, NULL, &attributes_shotTask);

  //StartUsb();

  /* Let's try to create SCREEN_DIR and find possible initial errors. */

  res = M_SDMakeDirectory(SCREEN_DIR);

  if (res != FR_OK && res != FR_EXIST)
  {
    switch (res)
    {
    case FR_NOT_READY:
      errs1 = "SD card not ready.";
      break;
    case FR_DENIED:
      errs1 = "SD card protected.";
      break;
    case FR_INT_ERR:
    case FR_DISK_ERR:
    default:
      errs1 = "SD card error.";
      break;
    }
    guiev.evcode = GUIEV_SD_REPORT;
    guiev.evval0 = res;
    guiev.evarg1 = errs1;
    guiev.evarg2 = "";
    postGuiEvent(&guiev);

    while (1) osDelay(100);
  }

  wait_time = osWaitForever;

  while (1)
  {
    REQUEST_CMD request;
    WADLIST *game;
    int qst;

    qst = osMessageQueueGet(reqcmdqId, &request, NULL, wait_time);
    if (qst != osOK)
    {
      debug_printf("qst = %d\n", qst);
      NVIC_SystemReset();
    }

    switch (request.cmd)
    {
    case REQ_VERIFY_SD:
      guiev.evcode = GUIEV_SD_REPORT;
      res = VerifySDCard((void *)&errs1, (void *)&errs2);
      guiev.evval0 = (res > 0)? 0 : res;
      guiev.evarg1 = errs1;
      guiev.evarg2 = errs2;
      postGuiEvent(&guiev);
      break;
    case REQ_VERIFY_FLASH:
      guiev.evcode = GUIEV_FLASH_REPORT;
      guiev.evval0 = VerifyFlash((void *)&errs1, (void *)&errs2);
      guiev.evarg1 = errs1;
      guiev.evarg2 = errs2;
      postGuiEvent(&guiev);
      break;
    case REQ_ERASE_FLASH:
      game = (WADLIST *) request.arg;
      CopyFlash(game, (uint32_t)res);
      break;
    case REQ_END_DOOM:
      wait_time = 3000;
      btapi_disconnect();
      break;
    case REQ_DUMMY:
      if (wait_time != osWaitForever)
        wait_time = 500;
      break;
    default:
      break;
    }
  }
}

void StartShotTask(void *argv)
{
  UINT nb;
  FIL *pfile;
  char fname[FNAME_LEN];

  pfile = NULL;

  while (1)
  {
    SCREENSHOT_CMD request;

    osMessageQueueGet(shotqId, &request, NULL, osWaitForever);

    switch (request.cmd)
    {
    case SCREENSHOT_CREATE:
      snprintf(fname, FNAME_LEN, "%s/sc%03d_%s.jpg", SCREEN_DIR, request.val, (char *)request.arg);
      debug_printf("SCREEN_CREATE %s\n", fname);
#define USE_FATFS
#ifdef USE_FATFS
      pfile = CreateJpegFile(fname);
#endif
      postMainRequest(REQ_DUMMY, NULL, 0);
      break;
    case SCREENSHOT_WRITE:
      debug_printf("SCREEN_WRITE %d @ %x\n", request.val, request.arg);
#ifdef USE_FATFS
      if (pfile)
      {
        f_write(pfile, request.arg, request.val, &nb);
      }
#endif
      save_jpeg_write_done();
      postMainRequest(REQ_DUMMY, NULL, 0);
      break;
    case SCREENSHOT_CLOSE:
      debug_printf("SCREEN_CLOSE\n");
#ifdef USE_FATFS
      if (pfile)
      {
        CloseJpegFile(pfile);
        pfile = NULL;
      }
#endif
      save_jpeg_finish();
      break;
    default:
      break;
    }
  }
}

void postMainRequest(int cmd, void *arg, int val)
{
  REQUEST_CMD request;

  request.cmd = cmd;
  request.arg = arg;
  request.val = val;
  osMessageQueuePut(reqcmdqId, &request, 0, 0);
}

void postShotRequest(int cmd, void *arg, int val)
{
  SCREENSHOT_CMD request;

  request.cmd = cmd;
  request.arg = arg;
  request.val = val;
  osMessageQueuePut(shotqId, &request, 0, 0);
}
