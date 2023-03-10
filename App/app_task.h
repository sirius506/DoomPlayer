#ifndef __APP_TASK_H__
#define __APP_TASK_H__

#include "lvgl.h"

/* Main task request commands definitions */

typedef enum {
  REQ_VERIFY_SD = 1,		// Verify SD card contents
  REQ_VERIFY_FLASH,		// Verify OCTO SPI flash contents
  REQ_ERASE_FLASH,		// Erase OCTO SPI flash contents
  REQ_COPY_FLASH,		// Copy SD game image onto SPI flash
  REQ_END_DOOM,
  REQ_DUMMY,
} REQ_CODE;

typedef struct {
  REQ_CODE  cmd;
  void      *arg;
  int       val;
} REQUEST_CMD;

typedef enum {
  SCREENSHOT_CREATE,		// Create new screen shot file
  SCREENSHOT_WRITE,		// Write JPEG data
  SCREENSHOT_CLOSE,		// Close jpeg file
} SHOTREQ_CODE;

typedef struct {
  SHOTREQ_CODE  cmd;
  void      *arg;
  int       val;
} SCREENSHOT_CMD;

/* GUI task event definitions */

typedef enum {
  GUIEV_TOUCH_INT = 1,
  GUIEV_DUALSENSE_READY,	// DualSense controller has found
  GUIEV_SD_REPORT,		// Report REQ_VERIFY_SD result
  GUIEV_FLASH_REPORT,		// Report REQ_VERIFY_FLASH result
  GUIEV_MPLAYER_START,		// Start Music Player
  GUIEV_SPLAYER_START,		// Start Sound Player
  GUIEV_MPLAYER_DONE,
  GUIEV_MUSIC_FINISH,
  GUIEV_ERASE_START,
  GUIEV_ERASE_REPORT,
  GUIEV_COPY_REPORT,
  GUIEV_GAME_START,
  GUIEV_CHEAT_BUTTON,
  GUIEV_FFT_UPDATE,
  GUIEV_PSEC_UPDATE,
  GUIEV_REBOOT,
  GUIEV_FLASH_GAME_SELECT,	// Flash game image selected
  GUIEV_SD_GAME_SELECT,		// SD game image selected
  GUIEV_REDRAW_START,
  GUIEV_DUALTEST_START,
  GUIEV_DUALTEST_UPDATE,
  GUIEV_DUALTEST_DONE,
  GUIEV_XDIR_INC,
  GUIEV_XDIR_DEC,
  GUIEV_YDIR_INC,
  GUIEV_YDIR_DEC,
  GUIEV_KBD_OK,
  GUIEV_KBD_CANCEL,
  GUIEV_CHEAT_SEL,
  GUIEV_ENDOOM,
  GUIEV_LVGL_CAPTURE,
  GUIEV_DOOM_CAPTURE,
} GUIEV_CODE;

typedef struct {
  GUIEV_CODE evcode;
  uint32_t   evval0;
  void       *evarg1;
  void       *evarg2;
} GUI_EVENT;

#define	OP_ERROR	1
#define	OP_START	2
#define	OP_PROGRESS	3
#define	OP_DONE		4

typedef struct s_wadprop {
  int length;
  uint32_t crcval;
  const char *wadname;
  const char *title;
} WADPROP;

typedef struct {
  const WADPROP *wadInfo;
  uint32_t fsize;
  char fname[30];
} WADLIST;

#define KBDEVENT_DOWN	0
#define	KBDEVENT_UP	1

typedef struct {
  uint8_t evcode;
  uint8_t asciicode;
  uint8_t doomcode;
} KBDEVENT;

extern void postGuiEvent(const GUI_EVENT *event);
extern void postGuiEventMessage(GUIEV_CODE evcode, uint32_t evval0, void *evarg1, void *evarg2);
extern void postMainRequest(int cmd, void *arg, int val);
extern void postShotRequest(int cmd, void *arg, int val);
extern int VerifySDCard(void **errString1, void **errString2);
extern int VerifyFlash(void **p1, void **p2);
extern void CopyFlash(WADLIST *list, uint32_t foffset);

extern lv_obj_t *dualtest_create();
extern void dualtest_update();
extern lv_obj_t *music_player_create(int dev_flag, lv_group_t *g, lv_style_t *btn_style, lv_indev_t *keypad_dev);
extern lv_obj_t * _lv_demo_music_main_create(lv_obj_t * parent, lv_group_t *g, lv_style_t *btn_style);

extern void _lv_demo_inter_pause_start();
extern void app_spectrum_update(int v);
extern void app_psec_update(int psec);

extern int ReadMusicList(char *filename);
extern void _lv_demo_music_list_btn_check(lv_obj_t *list, uint32_t track_id, bool state);
extern int fft_getband(int band);

extern void LoadMusicConfigs();

extern void StartLvglTask(void *argument);
extern void StartShotTask(void *argument);
extern void StartConsoleTask(void *argument);
extern void StartUsb();
extern void StartDoomTask(void *argument);
extern void Start_SDLMixer(int mode);
extern void music_process_stick(int evcode);
extern void sound_process_stick(int evcode);

extern KBDEVENT *kbd_get_event();
#endif
