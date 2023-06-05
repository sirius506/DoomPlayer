/**
 *  LVGL task and its driver
 */
#include <stdio.h>
#include "DoomPlayer.h"
#include "gamepad.h"
#include "board_if.h"
#include "app_task.h"
#include "app_sound.h"
#include "app_gui.h"
#include "usbh_hid.h"
#include "jpeg_if.h"
#include "audio_output.h"

#define USE_WAIT_CB

extern int doom_main(int argc, char **argv);

TASK_DEF(doomTask, 2400, osPriorityBelowNormal)

extern DMA2D_HandleTypeDef DMA2D_HANDLE;
extern LTDC_HandleTypeDef LTDC_HANDLE;

LV_IMG_DECLARE(btimg)

#define GUIEVQ_DEPTH     10

static uint8_t guievqBuffer[GUIEVQ_DEPTH * sizeof(GUI_EVENT)];

MESSAGEQ_DEF(guievq, guievqBuffer, sizeof(guievqBuffer))
SEMAPHORE_DEF(dma2dsem)

#define	PADKEYQ_DEPTH	5
static uint8_t padkeyBuffer[PADKEYQ_DEPTH * sizeof(lv_indev_data_t)];

MESSAGEQ_DEF(padkeyq, padkeyBuffer, sizeof(padkeyBuffer));

static osMessageQueueId_t  guievqId;
static osMessageQueueId_t  padkeyqId;
static osSemaphoreId_t dma2dsemId;
static osThreadId_t    doomtaskId;

extern void doom_send_cheat_key(char key);

const char * doom_argv[] = {
  "DiscoDoom",
  NULL,
};

static int fnum;

const GUI_EVENT reboot_event = {
  GUIEV_REBOOT,
  0,
  NULL,
  NULL
};

const WADPROP InvalidFlashGame =
 { 0, 0, NULL, "Game not written" };


/** I2S audio configuration is always availablle
 */
static const AUDIO_DEVCONF I2S_Audio_Conf = {
  .mix_mode = MIXER_I2S_OUTPUT|MIXER_FFT_ENABLE,
  .playRate = 44100,
  .numChan = 2,
  .pseudoRate = 44100,
  .pDriver = &i2s_output_driver,
};

/**
 *  USB audio configuration becames available when
 *  supported gamepad has detected.
 */
static const AUDIO_DEVCONF *usb_audio_devconf;
static AUDIO_CONF AudioConfiguration;

#define	current_audio_devconf	AudioConfiguration.devconf


/**
 * Draw buffers
 *
 * buf1_1 and buf1_2 are drawing buffer used by lvgl.
 * When lvgl flush the drawing buffer, its contents are send to the LCD controller.
 *
 * Note that thses buffers are reserved within RAMD1 section.
 */
static SECTION_AXISRAM lv_color_t buf1_1[LCD_WIDTH * TFT_DRAW_HEIGHT];
static SECTION_AXISRAM lv_color_t buf1_2[LCD_WIDTH * TFT_DRAW_HEIGHT];

static lv_disp_drv_t disp_drv;
static lv_disp_draw_buf_t disp_buf_1;
static int lvgl_active;

extern uint8_t lvgl_fb[];

static void dma2d_buffer_copy(void * dest_buf, lv_coord_t dest_stride, const lv_area_t * dest_area,
                     void * src_buf, lv_coord_t src_stride, const lv_area_t * src_area)
{
  int32_t dest_w = lv_area_get_width(dest_area);
  int32_t dest_h = lv_area_get_height(dest_area);

  SCB_CleanInvalidateDCache();

  DMA2D->CR = 0x0;
  DMA2D->FGPFCCR = CM_RGB565;
  DMA2D->FGMAR = (uint32_t)src_buf;
  DMA2D->FGOR = src_stride - dest_w;
  DMA2D->OMAR = (uint32_t)dest_buf;
  DMA2D->OOR = dest_stride - dest_w;
  DMA2D->NLR = (dest_w << DMA2D_NLR_PL_Pos) | (dest_h << DMA2D_NLR_NL_Pos);

#ifdef USE_WAIT_CB
  DMA2D->CR |= DMA2D_CR_TCIE | DMA2D_CR_START;
#else
  DMA2D->CR |= DMA2D_CR_START;
#endif
}

/*
 * Flush the content of the internal buffer the specific area on the display.
 */
static void ex_disp_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
#ifdef DISP_FLUSH_Pin
  HAL_GPIO_WritePin(DISP_FLUSH_Port, DISP_FLUSH_Pin, GPIO_PIN_SET);
#endif
  int32_t x1 = area->x1;
  int32_t x2 = area->x2;
  int32_t y1 = area->y1;
  int32_t y2 = area->y2;

  /* Return if the area is out of the screen */
  if ((x2 < 0) || (y2 < 0) || (x1 > LCD_WIDTH - 1) || (y1 > LCD_HEIGHT - 1))
  {
    lv_disp_flush_ready(drv);
    return;
  }

  dma2d_buffer_copy((lv_color_t *) lvgl_fb + area->y1 * LV_HOR_RES + area->x1,
        (lv_coord_t) LV_HOR_RES, area, color_p, area->x2 - area->x1 + 1, area);
#ifndef USE_WAIT_CB
  lv_disp_flush_ready(drv);
#ifdef DISP_FLUSH_Pin
  HAL_GPIO_WritePin(DISP_FLUSH_Port, DISP_FLUSH_Pin, GPIO_PIN_RESET);
#endif
#endif
}

static void ex_disp_clean_dcache(lv_disp_drv_t *drv)
{
  SCB_CleanInvalidateDCache();
}

static lv_indev_data_t tp_data;
static lv_indev_drv_t indev_drv;
static lv_indev_drv_t keydev_drv;
static lv_indev_t *keypad_dev;

static void touch_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
  static uint16_t lastx, lasty;
  *data = tp_data;
  if (data->state == LV_INDEV_STATE_RELEASED)
  {
    data->point.x = lastx;
    data->point.y = lasty;
  }
  else
  {
    lastx = data->point.x;
    lasty = data->point.y;
  }
  // debug_printf("touch: %d @ (%d, %d)\n", data->state, data->point.x, data->point.y);
}

static void keypad_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
  osStatus st;
  lv_indev_data_t psrc;

  st = osMessageQueueGet(padkeyqId, &psrc, NULL, 0);
  if (st == osOK)
  {
     *data = psrc;
  }
}

static void dma2dCplt(DMA2D_HandleTypeDef *hdma2d)
{
  osSemaphoreRelease(dma2dsemId);
}

void drv_wait_cb(lv_disp_drv_t *drv)
{
  osSemaphoreAcquire(dma2dsemId, osWaitForever);
#ifdef DISP_FLUSH_Pin
  HAL_GPIO_WritePin(DISP_FLUSH_Port, DISP_FLUSH_Pin, GPIO_PIN_RESET);
#endif
  lv_disp_flush_ready(drv);
}

void lvtask_initialize(LTDC_HandleTypeDef *hltdc)
{
  memset((void *)LV_MEM_ADR, 0, LV_MEM_SIZE);
  lv_init();

  DMA2D_HANDLE.State = HAL_DMA2D_STATE_READY;

  dma2dsemId = osSemaphoreNew(1, 1, &attributes_dma2dsem);
  HAL_DMA2D_RegisterCallback(&DMA2D_HANDLE, HAL_DMA2D_TRANSFERCOMPLETE_CB_ID, dma2dCplt);

  lv_disp_draw_buf_init(&disp_buf_1, buf1_1, buf1_2, LCD_WIDTH * TFT_DRAW_HEIGHT);

  lv_disp_drv_init(&disp_drv);

  disp_drv.hor_res = LCD_WIDTH;
  disp_drv.ver_res = LCD_HEIGHT;

#ifdef USE_WAIT_CB
  disp_drv.wait_cb = drv_wait_cb;
#endif
  disp_drv.flush_cb = ex_disp_flush;
  disp_drv.clean_dcache_cb = ex_disp_clean_dcache;

  disp_drv.draw_buf = &disp_buf_1;

  lv_disp_drv_register(&disp_drv);

  /* Initialize touch screen
   * Note that LVGL expect that touch device is initialized after the display.
   */
  Board_Touch_Init(LCD_WIDTH, LCD_HEIGHT);

  lv_indev_drv_init(&indev_drv);
  indev_drv.read_cb = touch_read;
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.scroll_throw = 20;
  lv_indev_drv_register(&indev_drv);

  /*
   * Dualsense keypad is handle as keypad device.
   */
  lv_indev_drv_init(&keydev_drv);
  keydev_drv.read_cb = keypad_read;
  keydev_drv.type = LV_INDEV_TYPE_KEYPAD;
  keypad_dev = lv_indev_drv_register(&keydev_drv);
}


void gui_timer_inc()
{
  if (lvgl_active)
    lv_tick_inc(1);
}

static lv_style_t style_title;

static void process_touch()
{
  Board_Touch_GetState(&tp_data);
 // debug_printf("Touch: %d @ (%d, %d)\n", tp->state, tp->point.x, tp->point.y);
}


void postGuiEvent(const GUI_EVENT *event)
{
  while (guievqId == NULL)
  {
    osDelay(10);
  }
  osMessageQueuePut(guievqId, event, 0, 0);
}

void postGuiEventMessage(GUIEV_CODE evcode, uint32_t evval0, void *evarg1, void *evarg2)
{
  GUI_EVENT ev;

  ev.evcode = evcode;
  ev.evval0 = evval0;
  ev.evarg1 = evarg1;
  ev.evarg2 = evarg2;

  while (guievqId == NULL)
  {
    osDelay(10);
  }
  osMessageQueuePut(guievqId, &ev, 0, 0);
}

const GUI_EVENT touch_event = {
  GUIEV_TOUCH_INT,
  0,
  NULL,
  NULL
};

static void reboot_event_cb(lv_event_t *e)
{
  postGuiEvent(&reboot_event);
}

/**
 * @brief Callback called when flash game has selected.
 */
static void flash_btn_event_cb(lv_event_t *e)
{
  WADPROP *pwad = lv_event_get_user_data(e);

  postGuiEventMessage(GUIEV_FLASH_GAME_SELECT, 0, pwad, NULL);
}

/**
 * @brief Callback called when SD game has selected.
 */
static void sd_btn_event_cb(lv_event_t *e)
{
  WADLIST *pwad = lv_event_get_user_data(e);

  debug_printf("%x : %s\n", pwad, pwad->wadInfo->title);
  postGuiEventMessage(GUIEV_SD_GAME_SELECT, 0, pwad, NULL);
}

/**
 * @brief Callback called when message box on the copy screen has clicked.
 */
static void copy_event_cb(lv_event_t *e)
{
  GUI_EVENT ev;
  uint16_t index;
  lv_obj_t * obj = lv_event_get_current_target(e);

  index = lv_msgbox_get_active_btn(obj);
  if (index == 0)
  {
    ev.evcode = GUIEV_ERASE_START;
    ev.evval0 = 0;
  }
  else
  {
    ev.evcode = GUIEV_REDRAW_START;
    ev.evval0 = 0;
  }
  ev.evarg1 = NULL;
  ev.evarg2 = NULL;
  postGuiEvent(&ev);
}

static void pairing_btn_event_cb(lv_event_t *e)
{
  uint16_t index;
  lv_obj_t * obj = lv_event_get_current_target(e);

  index = lv_msgbox_get_active_btn(obj);
  if (index == 0)
  {
    debug_printf("NEED PAIRING!\n");
    btapi_start_scan();
  }
  postGuiEventMessage(GUIEV_PAIRING_CLOSE, 0, NULL, NULL);
}

/**
 * @brief Called when Game menu button has clicked.
 */
static void menu_event_cb(lv_event_t *e)
{
  int code = (int) lv_event_get_user_data(e);

  postGuiEventMessage(code, 0, NULL, NULL);
}

static void game_event_cb(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);
  GUI_EVENT ev;

  if (code == LV_EVENT_CLICKED || code == LV_EVENT_SHORT_CLICKED)
  {
    ev.evcode = GUIEV_CHEAT_BUTTON;
    ev.evval0 = code;
    postGuiEvent(&ev);
  }
}

START_SCREEN StartScreen;
MENU_SCREEN  MenuScreen;
COPY_SCREEN  CopyScreen;
GAME_SCREEN  GameScreen;
SOUND_SCREEN SoundScreen;

static const char * const copy_buttons[] = { "Yes", "Cancel", "" };
static const char * const reboot_btn[] = { "Reboot", "" };
static const char * const pairing_btn[] = { "Yes", "Skip", "" };

/**
 * @brief Change audio button text when clicked.
 */
static void audio_button_handler(lv_event_t *e)
{
  lv_obj_t *btn = lv_event_get_target(e);
  lv_obj_t *label = (lv_obj_t *)lv_event_get_user_data(e);

  if (lv_obj_get_state(btn) & LV_STATE_CHECKED)
  {
    lv_label_set_text(label, "USB");
    current_audio_devconf = usb_audio_devconf;
  }
  else
  {
    lv_label_set_text(label, "I2S");
    current_audio_devconf = &I2S_Audio_Conf;
  }
}

/**
 * @brief Bluetooth button handler. Send disconnect event to BTstack.
 */
static void bt_button_handler(lv_event_t *e)
{
   btapi_disconnect();
}

/*
 * Called by hid_dualsense to send a LVGL keycode.
 */
void send_padkey(lv_indev_data_t *pdata)
{
  if (padkeyqId)
  {
    osMessageQueuePut(padkeyqId, pdata, 0, 0);
//debug_printf("padkey: %x\n", pdata->key);
  }
}

/*
 * Cheat code keyboard handler to detect OK and/or Cancel input
 */
static void keyboard_handler(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);
  GUI_EVENT event;

  
  event.evcode = 0;

  if (code == LV_EVENT_READY)
    event.evcode = GUIEV_KBD_OK;
  else if (code == LV_EVENT_CANCEL)
    event.evcode = GUIEV_KBD_CANCEL;

  if (event.evcode)
  {
    event.evval0 = 0;
    event.evarg1 = NULL;
    event.evarg2 = NULL;
    postGuiEvent(&event);
  }
}

static const char * const cheatcode_map[] = {
  "idfa", "idkfa", "iddqd", "idclip", "\n",
  "Cancel",
  "",
};


static void cheat_button_handler(lv_event_t *e)
{
  lv_obj_t *obj = lv_event_get_target(e);
  uint32_t id;
  const char *txt;

  //debug_printf("%s selected\n", txt);
  id = lv_btnmatrix_get_selected_btn(obj);
  txt = lv_btnmatrix_get_btn_text(obj, id);
  if (strncmp(txt, "Cancel", 6) == 0)
    txt = "";

  postGuiEventMessage(GUIEV_CHEAT_SEL, 0, (void *)txt, NULL);
}

/*
 * Temporary display flush routine to take screenshot
 */
static void gui_screenshot(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *colorp)
{
  if (area->x1 == 0 && area->y1 == 0)
  {
    /* At the begining of line, prepare JPEG encoder. */
    save_jpeg_start(fnum, "LVGL", LCD_WIDTH, LCD_HEIGHT);
  }
  dma2d_buffer_copy((lv_color_t *) lvgl_fb + area->y1 * LV_HOR_RES + area->x1,
        (lv_coord_t) LV_HOR_RES, area, colorp, area->x2 - area->x1 + 1, area);

  /* When we reach to end of the screen line, frame buffer has completely filled. */
  if (area->y2 >= LCD_HEIGHT-1)
  {
    SCB_CleanInvalidateDCache();
    save_jpeg_data(lvgl_fb, LCD_WIDTH, LCD_HEIGHT);
  }
  lv_disp_flush_ready(disp);
}

static void lvgl_capture()
{
  lv_disp_t *disp = lv_disp_get_default();
  void (*flush_cb)(struct _lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p);


  flush_cb = disp->driver->flush_cb;	// store original callback first
  disp->driver->flush_cb = gui_screenshot;  // replace the callback function

  lv_obj_invalidate(lv_scr_act());
  lv_refr_now(NULL);			// Force a call our screenshot callback
  disp->driver->flush_cb = flush_cb;     // restore the original callback function
}

void lv_timer_handler_hook()
{
#ifdef LV_TIMER_Pin
  HAL_GPIO_WritePin(LV_TIMER_Port, LV_TIMER_Pin, GPIO_PIN_SET);
#endif
  lv_timer_handler();
#ifdef LV_TIMER_Pin
  HAL_GPIO_WritePin(LV_TIMER_Port, LV_TIMER_Pin, GPIO_PIN_RESET);
#endif
}

/* Space, USB, Space, Bluetooth, Space, Battery */
static char icon_label_string[13] = {
  ' ',			// 0
  ' ', ' ', ' ',	// 1
  ' ',			// 4
  ' ', ' ', ' ',	// 5
  ' ',			// 8
  ' ', ' ', ' ',	// 9
  0x00 };

static const char *icon_map[] = {
  LV_SYMBOL_BATTERY_EMPTY,
  LV_SYMBOL_BATTERY_1,
  LV_SYMBOL_BATTERY_2,
  LV_SYMBOL_BATTERY_3,
  LV_SYMBOL_BATTERY_FULL,
};

GAMEPAD_INFO nullPad = {
  .name = "Dummy Controller",
  .hid_mode = HID_MODE_LVGL,
  .padDriver = NULL,
};

/*
 * Our LVGL processing task. We'll create GUI screens and
 * process GUI update request events.
 */
void StartLvglTask(void *argument)
{
  osStatus st;
  char sbuff[70];
  lv_obj_t *label;
  lv_obj_t *tlabel;
  lv_obj_t *sound_list;
  WADLIST *wadlist;
  WADPROP *flash_game;
  WADPROP *sel_flash_game;
  WADLIST *sel_sd_game;
  lv_obj_t *btn_row;
  lv_obj_t *fbutton;
  lv_style_t game_style;
  lv_style_t style_menubtn;
  int reboot_prompt;
  START_SCREEN *starts = &StartScreen;
  MENU_SCREEN *menus = &MenuScreen;
  COPY_SCREEN *copys = &CopyScreen;
  GAME_SCREEN *games = &GameScreen;
  SOUND_SCREEN *sounds = &SoundScreen;
  int timer_interval;
  const GUI_LAYOUT *layout = &GuiLayout;
  extern bool D_GrabMouseCallback();
  const char *kbtext;
  lv_img_dsc_t *endimg;
  lv_msgbox_t *mbox;
  uint8_t *bp;
  int img_offset;
  lv_obj_t *icon_label;
  uint16_t icon_value;
  GAMEPAD_INFO *padInfo;

  padInfo = &nullPad;
  fnum = 0;
  icon_value = 0;

  static lv_group_t *g;

  guievqId = osMessageQueueNew(GUIEVQ_DEPTH, sizeof(GUI_EVENT), &attributes_guievq);
  padkeyqId = osMessageQueueNew(PADKEYQ_DEPTH, sizeof(lv_indev_data_t), &attributes_padkeyq);

  init_jpeg();
  lvtask_initialize(&LTDC_HANDLE);
  lvgl_active = 1;

  current_audio_devconf = &I2S_Audio_Conf;

  lv_style_init(&style_title);
  lv_style_set_text_font(&style_title,  layout->font_title);

  lv_style_init(&style_menubtn);
  lv_style_set_outline_width(&style_menubtn, layout->mb_olw);

  icon_label = lv_label_create(lv_layer_top());
  //lv_label_set_text(icon_label, " " LV_SYMBOL_USB " " LV_SYMBOL_BLUETOOTH);
  lv_label_set_text(icon_label, (const char *)icon_label_string);
  starts->screen = lv_obj_create(NULL);
  menus->screen = lv_obj_create(NULL);
  copys->screen = lv_obj_create(NULL);
  games->screen = lv_obj_create(NULL);
  sounds->screen = lv_obj_create(NULL);

  /* Create initial startup screen */

  lv_scr_load(starts->screen);

  starts->title = lv_label_create(starts->screen);
  lv_obj_add_style(starts->title, &style_title, 0);
  lv_label_set_text(starts->title, "Doom Player");
  lv_obj_align(starts->title, LV_ALIGN_TOP_MID, 0, lv_pct(7));

  menus->title = lv_label_create(menus->screen);
  lv_obj_add_style(menus->title, &style_title, 0);
  lv_label_set_text(menus->title, "");
  lv_obj_align(menus->title, LV_ALIGN_TOP_MID, 0, lv_pct(7));

  copys->title = lv_label_create(copys->screen);
  lv_obj_add_style(copys->title, &style_title, 0);
  lv_label_set_text(copys->title, "");
  lv_obj_align(copys->title, LV_ALIGN_TOP_MID, 0, lv_pct(7));

  copys->operation = lv_label_create(copys->screen);
  lv_label_set_text(copys->operation, "");
  lv_obj_align(copys->operation, LV_ALIGN_TOP_MID, 0, lv_pct(30));

  copys->fname = lv_label_create(copys->screen);
  lv_label_set_text(copys->fname, "");
  lv_obj_align(copys->fname, LV_ALIGN_TOP_MID, 0, lv_pct(38));

  /* Prepare Audio selection container, but make it invisible */

  menus->cont_audio = lv_obj_create(menus->screen);
  lv_obj_set_size(menus->cont_audio, layout->audio_box_width, layout->audio_box_height);
  lv_obj_align(menus->cont_audio, LV_ALIGN_CENTER, 0, layout->audio_box_yoffset);
  lv_obj_add_flag(menus->cont_audio, LV_OBJ_FLAG_HIDDEN);

  /* Prepare Bluetooth image buffon. */

  menus->cont_bt = lv_imgbtn_create(menus->screen);
  lv_imgbtn_set_src(menus->cont_bt, LV_IMGBTN_STATE_RELEASED, NULL, &btimg, NULL);
  lv_obj_align(menus->cont_bt, LV_ALIGN_CENTER, 0, layout->audio_box_yoffset);
  lv_obj_set_width(menus->cont_bt, LV_SIZE_CONTENT);
  lv_obj_add_event_cb(menus->cont_bt, bt_button_handler, LV_EVENT_CLICKED, NULL);
  lv_obj_add_flag(menus->cont_bt, LV_OBJ_FLAG_HIDDEN);

  /* Create Game screen and fill with CHROMA_KEY color,
   * then it will become transparent.
   */

  lv_style_init(&game_style);
  lv_style_set_bg_color(&game_style, LV_COLOR_CHROMA_KEY);
  lv_obj_add_style(games->screen, &game_style, 0);
  games->cheat_btn = lv_btn_create(games->screen);
  lv_obj_set_size(games->cheat_btn, 200, 100);
  lv_obj_center(games->cheat_btn);
  lv_obj_add_event_cb(games->cheat_btn, game_event_cb, LV_EVENT_ALL, (void *)0);

  /* Prepare virtual keyboard. We'd like to draw the keyboard within DOOM screen area. */

  games->kbd = lv_keyboard_create(games->screen);
  lv_obj_align(games->kbd, LV_ALIGN_BOTTOM_MID, 0, -(LCD_HEIGHT-DOOM_SCHEIGHT)/2);
  lv_obj_set_width(games->kbd, DOOM_SCWIDTH);
  games->ta = lv_textarea_create(games->screen);	// Text area
  lv_obj_align(games->ta, LV_ALIGN_TOP_MID, 0, (LCD_HEIGHT-DOOM_SCHEIGHT)/2 + 60);
  lv_textarea_set_one_line(games->ta, true);
  lv_textarea_set_max_length(games->ta, 16);
  lv_obj_add_state(games->ta, LV_STATE_FOCUSED);
  lv_keyboard_set_textarea(games->kbd, games->ta);
  lv_obj_add_event_cb(games->kbd, keyboard_handler, LV_EVENT_ALL, (void *)0);
  lv_obj_add_flag(games->kbd, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(games->ta, LV_OBJ_FLAG_HIDDEN);

  /* Prepare cheat buttons */
  games->cheat_code = lv_btnmatrix_create(games->screen);
  lv_btnmatrix_set_map(games->cheat_code, (const char **)cheatcode_map);
  lv_obj_align(games->cheat_code, LV_ALIGN_TOP_MID, 0, lv_pct(40));
  lv_obj_add_event_cb(games->cheat_code, cheat_button_handler, LV_EVENT_CLICKED, NULL);
  lv_obj_add_flag(games->cheat_code, LV_OBJ_FLAG_HIDDEN);

  wadlist = NULL;
  tlabel = NULL;
  flash_game = NULL;
  sel_sd_game = NULL;
  menus->sub_scr = NULL;
  sound_list = NULL;

  postMainRequest(REQ_VERIFY_SD, NULL, 0);	// Start SD card verification

  reboot_prompt = 0;
  timer_interval = 3;
  lv_obj_t *pmobj = NULL;

  while (1)
  {
    GUI_EVENT event;

    st = osMessageQueueGet(guievqId, &event, NULL, timer_interval);
    if (st == osOK)
    {
      switch (event.evcode)
      {
      case GUIEV_TOUCH_INT:
        process_touch();
        break;
      case GUIEV_PAIRING_OPEN:
        if (reboot_prompt == 0)
        {
          pmobj = lv_msgbox_create(NULL, "Bluetooth dongle found.", "Enter pairing mode?", (const char **)pairing_btn, false);
          lv_obj_center(pmobj);
          lv_obj_set_width(pmobj, W_PERCENT(40));
          lv_obj_add_event_cb(pmobj, pairing_btn_event_cb, LV_EVENT_CLICKED, NULL);
        }
        break;
      case GUIEV_PAIRING_CLOSE:
        if (pmobj)
        {
          lv_msgbox_close(pmobj);
          pmobj = NULL;
        }
        break;
      case GUIEV_GAMEPAD_READY:
        padInfo = (GAMEPAD_INFO *)event.evarg1;
        debug_printf("%s Detected.\n", padInfo->name);
        break;
      case GUIEV_BLUETOOTH_READY:
        if (event.evval0 == 0)
        {
          /* Bluetooth disconnected. Hide the image button. */
          lv_obj_add_flag(menus->cont_bt, LV_OBJ_FLAG_HIDDEN);
          lv_obj_add_state(menus->btn_dual, LV_STATE_DISABLED);
        }
        else
        {
          /* Bluetooth connection established. Display the image button. */
          lv_obj_clear_flag(menus->cont_bt, LV_OBJ_FLAG_HIDDEN);
          lv_obj_clear_state(menus->btn_dual, LV_STATE_DISABLED);
        }
        break;
      case GUIEV_USB_AUDIO_READY:
        if (Mix_Started() == 0)
        {
          /*
           * Audio playback task is not running, yet.
           * Make audio selection visible.
           */
          usb_audio_devconf = (AUDIO_DEVCONF *)event.evarg1;
          lv_obj_clear_flag(menus->cont_audio, LV_OBJ_FLAG_HIDDEN);
          if (menus->btn_dual)
            lv_obj_clear_state(menus->btn_dual, LV_STATE_DISABLED);
        }
        break;
      case GUIEV_MUSIC_FINISH:
        _lv_demo_inter_pause_start();
        break;
      case GUIEV_SD_REPORT:		// SD card verification has finished
        if (event.evval0 == 0)
        {
          /* SD card verify successfull. */

          label = lv_label_create(starts->screen);
          lv_label_set_text(label, LV_SYMBOL_OK " SD card verified.");
          lv_obj_align_to(label, starts->title, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

          /* Send SPI flash verify command. */

          tlabel = lv_label_create(starts->screen);
          lv_label_set_text(tlabel, " Checking SPI Flash...");
          lv_obj_align_to(tlabel, label, LV_ALIGN_OUT_BOTTOM_MID, 0, H_PERCENT(2));
          wadlist = (WADLIST *)event.evarg1;

          postMainRequest(REQ_VERIFY_FLASH, NULL, 0);

          /* Start spinner */

          starts->spinner = lv_spinner_create(starts->screen, 1000, 60);
          lv_obj_set_size(starts->spinner, W_PERCENT(20), W_PERCENT(20));
          lv_obj_align(starts->spinner, LV_ALIGN_TOP_MID, 0, H_PERCENT(45));
          lv_obj_set_style_arc_width(starts->spinner, layout->spinner_width, LV_PART_MAIN);
          lv_obj_set_style_arc_width(starts->spinner, layout->spinner_width, LV_PART_INDICATOR);
        }
        else
        {
          /* SD card verification failed. Show message box to reboot. */

          sprintf(sbuff, "%s %s", (char *)event.evarg1, (char *)event.evarg2);
          starts->mbox = lv_msgbox_create(NULL, "Bad SD Card", sbuff, (const char **)reboot_btn, false);
          starts->btn = lv_msgbox_get_btns(starts->mbox);
          lv_obj_update_layout(starts->btn);
          debug_printf("btn size = %d x %d\n", lv_obj_get_width(starts->btn), lv_obj_get_height(starts->btn));

          g = lv_group_create();
          lv_group_add_obj(g, starts->btn);
          lv_indev_set_group(keypad_dev, g);
          lv_obj_add_event_cb(starts->mbox, reboot_event_cb, LV_EVENT_PRESSED, NULL);
          lv_obj_center(starts->mbox);
          reboot_prompt = 1;
        }
        break;
      case GUIEV_FLASH_REPORT:			// SPI flash verification finished
        lv_obj_del(starts->spinner);	// Stop spinner

        {
          static lv_style_t style_flashbutton;
          static lv_style_t style_sdbutton;
          WADLIST *sdgame;

          lv_style_init(&style_flashbutton);
          lv_style_init(&style_sdbutton);
          lv_style_set_bg_color(&style_sdbutton, lv_palette_main(LV_PALETTE_ORANGE));
          //lv_style_set_outline_width(&style_sdbutton, layout->mb_olw);

          if (event.evval0 == 0)
          {
            lv_style_set_bg_color(&style_flashbutton, lv_palette_main(LV_PALETTE_LIGHT_GREEN));
            lv_label_set_text(tlabel, LV_SYMBOL_OK " SPI Flash verified.");

            /* Display game title availabe on the Flash */

            flash_game = (WADPROP *)event.evarg1;
          }
          else
          {
            lv_style_set_bg_color(&style_flashbutton, lv_palette_main(LV_PALETTE_GREY));
            lv_label_set_text(tlabel, LV_SYMBOL_CLOSE " SPI Flash invalid.");
            flash_game = (WADPROP *)&InvalidFlashGame;
          }

          fbutton = lv_btn_create(starts->screen);
          lv_obj_align(fbutton, LV_ALIGN_TOP_MID,  0, lv_pct(35));
          if (event.evval0 == 0)
          {
            lv_obj_add_style(fbutton, &style_flashbutton, 0);
            lv_obj_add_event_cb(fbutton, flash_btn_event_cb, LV_EVENT_CLICKED, flash_game);
          }
          else
          {
            lv_obj_add_style(fbutton, &style_flashbutton, 0);
          }
          lv_obj_set_style_outline_width(fbutton, layout->mb_olw / 2, LV_STATE_FOCUS_KEY);

          lv_obj_t *blabel = lv_label_create(fbutton);
          lv_label_set_text(blabel, flash_game->title);
          lv_obj_center(blabel);

          /* Display titles on SD card */

          btn_row = lv_obj_create(starts->screen);
          lv_obj_set_size(btn_row, W_PERCENT(54), lv_pct(45));
          lv_obj_align_to(btn_row, fbutton, LV_ALIGN_OUT_BOTTOM_MID, 0, H_PERCENT(4));
          lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_COLUMN);

          WADLIST *wp;

          starts->ing = lv_group_create();
          lv_indev_set_group(keypad_dev, starts->ing);

          lv_group_add_obj(starts->ing, fbutton);

          lv_gridnav_add(btn_row, LV_GRIDNAV_CTRL_ROLLOVER);
          lv_group_add_obj(starts->ing, btn_row);

          /* Create buttons for IWAD files on the SD card */

          for (wp = wadlist; wp->wadInfo; wp++)
          {
            sdgame = wp;
            if (sdgame->wadInfo != flash_game)	// Don't create button if the IWAD is on the SPI flash
            {
              lv_obj_t *obj;
              lv_obj_t *label;

              obj = lv_btn_create(btn_row);
              lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
              lv_obj_add_style(obj, &style_sdbutton, 0);
              lv_obj_add_event_cb(obj, sd_btn_event_cb, LV_EVENT_CLICKED, sdgame);
              lv_obj_set_style_outline_width(obj, layout->mb_olw / 2, LV_STATE_FOCUS_KEY);

              label = lv_label_create(obj);
              lv_label_set_text(label, sdgame->wadInfo->title);
              lv_obj_center(label);
            }
          }
          if (event.evval0 == 0)
            StartUsb();
        }
        break;
      case GUIEV_FLASH_GAME_SELECT:
        /* Selected game reside on the SPI flash. */

        sel_flash_game = (WADPROP *)event.evarg1;
        debug_printf("GAME: %s\n", sel_flash_game->title);

        lv_label_set_text(menus->title, sel_flash_game->title);

        sounds->ing = lv_group_create();
        //sound_screen_create(sounds->screen, sounds->ing, &style_menubtn);

        menus->ing = lv_group_create();
        lv_indev_set_group(keypad_dev, menus->ing);

        /* Create Menu buttons */

        lv_obj_t *cont = lv_obj_create(menus->screen);
        lv_obj_remove_style_all(cont);
        lv_obj_set_size(cont, LCD_WIDTH - (LCD_WIDTH/8), layout->mb_height + 7);

        lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, layout->mb_yoffset);

#define	ADD_MENU_BUTTON(btn_name, btn_string, btn_code) \
	menus->btn_name = lv_btn_create(cont); \
        lv_obj_add_style(menus->btn_name, &style_menubtn, LV_STATE_FOCUS_KEY); \
        lv_obj_set_height(menus->btn_name, layout->mb_height); \
        label = lv_label_create(menus->btn_name); \
        lv_label_set_text(label, btn_string); \
        lv_obj_center(label); \
        lv_obj_add_event_cb(menus->btn_name, menu_event_cb, LV_EVENT_CLICKED, (void *)btn_code);  \
        lv_group_add_obj(menus->ing, menus->btn_name);

	ADD_MENU_BUTTON(btn_music, "Music", GUIEV_MPLAYER_START)
	ADD_MENU_BUTTON(btn_sound, "Sound", GUIEV_SPLAYER_START)
	ADD_MENU_BUTTON(btn_dual, "DualSense\n Demo", GUIEV_DUALTEST_START)
	ADD_MENU_BUTTON(btn_game, "Game", GUIEV_GAME_START)

        if (padInfo->padDriver == NULL)
        {
          /* DualSense is not detected, yet. Disable DualSense demo button. */
          lv_obj_add_state(menus->btn_dual, LV_STATE_DISABLED);
        }

        lv_obj_set_flex_flow(menus->cont_audio, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(menus->cont_audio, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        /* Create Audio putput selection button and container */

        lv_obj_t *alabel = lv_label_create(menus->cont_audio);
        lv_label_set_text(alabel, "Audio Output");

        menus->btn_audio = lv_btn_create(menus->cont_audio);
        lv_obj_t *blabel = lv_label_create(menus->btn_audio);
        lv_obj_set_height(menus->btn_audio, layout->audio_button_height);
        lv_obj_add_flag(menus->btn_audio, LV_OBJ_FLAG_CHECKABLE);
        if (current_audio_devconf == &I2S_Audio_Conf)
        {
          lv_obj_clear_state(menus->btn_audio, LV_STATE_CHECKED);
          lv_label_set_text(blabel, "I2S");
        }
        else
        {
          lv_obj_add_state(menus->btn_audio, LV_STATE_CHECKED);
          lv_label_set_text(blabel, "USB");
        }
        lv_obj_add_event_cb(menus->btn_audio, audio_button_handler, LV_EVENT_VALUE_CHANGED, blabel);
        lv_obj_center(blabel);
        lv_group_add_obj(menus->ing, menus->btn_audio);
        lv_scr_load(menus->screen);
        lv_obj_del(starts->screen);
        break;

      case GUIEV_SD_GAME_SELECT:
        sel_sd_game = (WADLIST *)event.evarg1;
        debug_printf("GAME: %s\n", sel_sd_game->wadInfo->title);

        /* Selected game resind on the SD card.
         * Copy it into the SPI flash.
         */
        lv_label_set_text(copys->title, sel_sd_game->wadInfo->title);
        lv_scr_load(copys->screen);

        copys->mbox = lv_msgbox_create(copys->screen, "Copy Game Image",
             "Are you sure to erase & re-write flash contents?", (const char **)copy_buttons, false);
        lv_obj_add_event_cb(copys->mbox, copy_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_center(copys->mbox);
        g = lv_group_create();
        mbox = (lv_msgbox_t *)copys->mbox;
        lv_group_add_obj(g, mbox->btns);
        lv_indev_set_group(keypad_dev, g);
        break;
      case GUIEV_ERASE_START:
        lv_obj_del(copys->mbox);
        lv_timer_handler_hook();
        copys->bar = lv_bar_create(copys->screen);
        lv_obj_set_size(copys->bar, lv_pct(40), lv_pct(8));
        lv_obj_align(copys->bar, LV_ALIGN_TOP_MID, 0, lv_pct(55));
        lv_bar_set_value(copys->bar, 0, LV_ANIM_OFF);
        postMainRequest(REQ_ERASE_FLASH, sel_sd_game, 0);
        lv_timer_handler_hook();
        break;
      case GUIEV_REDRAW_START:
	/*
         * Copy operation has aborted.
         * Redraw start screen.
         */
        lv_scr_load(starts->screen);
        lv_indev_set_group(keypad_dev, starts->ing);
        break;
      case GUIEV_REBOOT:
        btapi_disconnect();
        osDelay(200);
        NVIC_SystemReset();
        break;
      case GUIEV_ERASE_REPORT:
        switch (event.evval0)
        {
        case OP_START:
          lv_label_set_text(copys->operation, "Erasing SPI Flash..");
          copys->progress = lv_label_create(copys->screen);
          lv_label_set_text(copys->progress, "");
          lv_obj_align_to(copys->progress, copys->bar, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
          break;
        case OP_ERROR:
          sprintf(sbuff, "%s %s", (char *)event.evarg1, (char *)event.evarg2);
          copys->mbox = lv_msgbox_create(NULL, "Flash Erase Error", sbuff, (const char **)reboot_btn, false);
          mbox = (lv_msgbox_t *)copys->mbox;
          g = lv_group_create();
          lv_group_add_obj(g, mbox->btns);
          lv_indev_set_group(keypad_dev, g);
          lv_obj_add_event_cb(starts->mbox, reboot_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
          lv_obj_center(starts->mbox);
          reboot_prompt = 1;
          break;
        case OP_PROGRESS:
          sprintf(sbuff, "%d%%", (int)event.evarg1);
          lv_bar_set_value(copys->bar, (int)event.evarg1, LV_ANIM_OFF);
          lv_label_set_text(copys->progress, sbuff);
          lv_timer_handler_hook();
          break;
        }
        break;
      case GUIEV_COPY_REPORT:
        switch (event.evval0)
        {
        case OP_START:
          lv_label_set_text(copys->operation, "Copying to SPI Flash..");
          if (event.evarg1)
            lv_label_set_text(copys->fname, event.evarg1);
          else
            lv_label_set_text(copys->fname, "");
          lv_bar_set_value(copys->bar, 0, LV_ANIM_OFF);
          break;
        case OP_ERROR:
          sprintf(sbuff, "%s %s", (char *)event.evarg1, (char *)event.evarg2);
          copys->mbox = lv_msgbox_create(NULL, "Flash Erase Error", sbuff, (const char **)reboot_btn, false);
          g = lv_group_create();
          mbox = (lv_msgbox_t *)copys->mbox;
          lv_group_add_obj(g, mbox->btns);
          lv_indev_set_group(keypad_dev, g);
          lv_obj_center(copys->mbox);
          lv_obj_center(copys->mbox);
          reboot_prompt = 1;
          break;
        case OP_PROGRESS:
          sprintf(sbuff, "%d%%", (int)event.evarg1);
          lv_bar_set_value(copys->bar, (int)event.evarg1, LV_ANIM_OFF);
          lv_label_set_text(copys->progress, sbuff);
          lv_timer_handler_hook();
          break;
        case OP_DONE:
          lv_obj_del(copys->operation);
          lv_obj_del(copys->progress);
          lv_obj_del(copys->bar);
          copys->mbox = lv_msgbox_create(NULL, "Copy done", "Reboot to activate new game.", (const char **)reboot_btn, false);
          g = lv_group_create();
          mbox = (lv_msgbox_t *)copys->mbox;
          lv_group_add_obj(g, mbox->btns);
          lv_indev_set_group(keypad_dev, g);
          lv_obj_add_event_cb(copys->mbox, reboot_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
          lv_obj_center(copys->mbox);
          reboot_prompt = 1;
          break;
        }
        break;
      case GUIEV_SPLAYER_START:
        Start_SDLMixer(&AudioConfiguration);
        if (sound_list == NULL)
        {
          sound_list = sound_screen_create(sounds->screen, sounds->ing, &style_menubtn);
        }
        lv_obj_add_flag(menus->cont_audio, LV_OBJ_FLAG_HIDDEN);
        lv_scr_load(sounds->screen);
        lv_indev_set_group(keypad_dev, sounds->ing);
        break;
      case GUIEV_MPLAYER_START:
        if (menus->play_scr == NULL)
        {
          menus->player_ing = lv_group_create();
          lv_indev_set_group(keypad_dev, menus->player_ing);
          Start_SDLMixer(&AudioConfiguration);
          menus->play_scr = music_player_create(current_audio_devconf, menus->player_ing, &style_menubtn, keypad_dev);
          lv_obj_add_flag(menus->cont_audio, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
          lv_scr_load(menus->play_scr);
          lv_indev_set_group(keypad_dev, menus->player_ing);
        }
        break;
      case GUIEV_DUALTEST_START:
        if (padInfo->padDriver)
        {
          padInfo->padDriver->ResetFusion();
          GamepadHidMode(padInfo, HID_MODE_TEST);
        }
        if (menus->sub_scr == NULL)
          menus->sub_scr = dualtest_create(g);
        lv_scr_load(menus->sub_scr);
        break;
      case GUIEV_DUALTEST_UPDATE:
        dualtest_update(event.evarg1, event.evval0);
        break;
      case GUIEV_DUALTEST_DONE:
        GamepadHidMode(padInfo, HID_MODE_LVGL);
        /* Fall down to .. */
      case GUIEV_MPLAYER_DONE:
        /*
         * Restore menu screen and its input group/
         */
        lv_scr_load(menus->screen);
        lv_indev_set_group(keypad_dev, menus->ing);
        break;
      case GUIEV_GAME_START:
        if (g)
          lv_group_del(g);
        lv_obj_add_flag(icon_label, LV_OBJ_FLAG_HIDDEN);
        GamepadHidMode(padInfo, HID_MODE_DOOM);
        Mix_FFT_Disable();
        osThreadYield();
        Mix_HaltMusic();		// Make sure to stop music playing
        osThreadYield();
        lv_scr_load(games->screen);

        Board_DoomModeLCD();

        if (menus->sub_scr)
          lv_obj_del(menus->sub_scr);
        osDelay(100);
        doomtaskId = osThreadNew(StartDoomTask, &doom_argv, &attributes_doomTask);
        break;
      case GUIEV_FFT_UPDATE:
        if (padInfo->hid_mode != HID_MODE_DOOM)
          app_spectrum_update(event.evval0);
        break;
      case GUIEV_PSEC_UPDATE:
        app_psec_update(event.evval0);
        break;
      case GUIEV_CHEAT_BUTTON:
        if (D_GrabMouseCallback())	// Not in menu/demo mode
        {
          lv_obj_add_flag(games->cheat_btn, LV_OBJ_FLAG_HIDDEN);
          if ((event.evval0 == LV_EVENT_CLICKED) && lv_obj_has_flag(games->cheat_code, LV_OBJ_FLAG_HIDDEN))
          {
            lv_textarea_set_text(games->ta, "id");
            lv_obj_clear_flag(games->kbd, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(games->ta, LV_OBJ_FLAG_HIDDEN);
          }
          else
          {
            lv_obj_clear_flag(games->cheat_code, LV_OBJ_FLAG_HIDDEN);
          }
          Board_GUI_LayerVisible(220);
        }
        break;
      case GUIEV_KBD_OK:
        kbtext = lv_textarea_get_text(games->ta);
        if (strncmp(kbtext, "id", 2) == 0)	/* Cheat code must start width "id" */
        {
          while (*kbtext)
          {
            doom_send_cheat_key(*kbtext++);
          }
        }
        lv_obj_add_flag(games->kbd, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(games->ta, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(games->cheat_btn, LV_OBJ_FLAG_HIDDEN);
        Board_GUI_LayerInvisible();
        break;
      case GUIEV_KBD_CANCEL:
        lv_obj_clear_flag(games->cheat_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(games->kbd, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(games->ta, LV_OBJ_FLAG_HIDDEN);
        Board_GUI_LayerInvisible();
        break;
      case GUIEV_CHEAT_SEL:
        kbtext = event.evarg1;

        while (*kbtext)
        {
          doom_send_cheat_key(*kbtext++);
        }
        lv_obj_clear_flag(games->cheat_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(games->cheat_code, LV_OBJ_FLAG_HIDDEN);
        Board_GUI_LayerInvisible();
        break;
      case GUIEV_ENDOOM:
        lv_style_set_bg_color(&game_style, lv_color_white());
        lv_obj_add_style(games->screen, &game_style, 0);
        games->img = lv_img_create(games->screen);
        lv_obj_remove_style_all(games->screen);
        lv_obj_remove_style_all(games->img);

        /* Prepare ENDOOM screen as LVGL image */
        endimg = Board_Endoom(event.evarg1);
        lv_img_set_src(games->img, endimg);
        img_offset = (LCD_HEIGHT - endimg->header.h)/2;
        debug_printf("%dx%d --> %d\n", endimg->header.w, endimg->header.h, img_offset);
        lv_obj_align(games->img, LV_ALIGN_TOP_MID, 0, img_offset);
        Board_GUI_LayerVisible(255);			// Enable LVGL screen layer
        //lv_timer_handler();
        Board_DOOM_LayerInvisible();			// Disable DOOM layer
        break;
      case GUIEV_LVGL_CAPTURE:		/* Take snapshot of LVGL screen */
        lvgl_capture();
        fnum++;
        fnum %= 1000;
        break;
      case GUIEV_DOOM_CAPTURE:		/* Take snapshot of DOOM screen */
        bp = Board_DoomCapture();
        save_jpeg_start(fnum, "DOOM", DOOM_SCWIDTH, DOOM_SCHEIGHT);
        save_jpeg_data(bp, DOOM_SCWIDTH, DOOM_SCHEIGHT);
        lvgl_capture();
        fnum++;
        fnum %= 1000;
        break;
      case GUIEV_XDIR_INC:
      case GUIEV_XDIR_DEC:
      case GUIEV_YDIR_INC:
      case GUIEV_YDIR_DEC:
        if (lv_scr_act() == sounds->screen)
          sound_process_stick(event.evcode);
        else if (lv_scr_act() == menus->play_scr)
          music_process_stick(event.evcode);
        break;
      case GUIEV_ICON_CHANGE:
        {
          int ival;
          char *sp;

          ival = event.evval0;
          if (ival & ICON_BATTERY)
          {
            icon_value &= ~ICON_BATTERY_MASK;
            icon_value |= ival & ICON_BATTERY_MASK;
            icon_value |= ICON_BATTERY;
          }
          else
          {
            if (ival & ICON_SET)
              icon_value |= (ival & (ICON_USB|ICON_BLUETOOTH));
            else
            {
              icon_value &= ~(ival & (ICON_USB|ICON_BLUETOOTH));

              if (ival & ICON_BLUETOOTH)
              {
                /* If BT connection is lost,
                 * we no longer able to get battery level.
                 */
                icon_value &= ~ICON_BATTERY;
              }
            }
          }
          sp = icon_label_string;
          *sp++ = ' ';
          if (icon_value & ICON_USB)
          {
            strncpy(sp, LV_SYMBOL_USB, 3);
            sp += 3;
            *sp++ = ' ';
          }
          if (icon_value & ICON_BLUETOOTH)
          {
            strncpy(sp, LV_SYMBOL_BLUETOOTH, 3);
            sp += 3;
            *sp++ = ' ';
          }
          if (icon_value & ICON_BATTERY)
          {
            strncpy(sp, icon_map[icon_value & ICON_BATTERY_MASK], 3);
            sp += 3;
          }
          *sp = 0;
          lv_label_set_text(icon_label, (const char *)icon_label_string);
        }
        break;
      default:
        break;
      }
    }
    else
    {
      lv_timer_handler_hook();
    }
  }
} 

void PostAppTouchEvent()
{
  postGuiEvent(&touch_event);
}

void StartDoomTask(void *argument)
{
  doom_main(1, (char **)argument);
  while (1) osDelay(100);
}

void app_endoom(uint8_t *bp)
{
  postGuiEventMessage(GUIEV_ENDOOM, 0, bp, NULL);
}

void app_screenshot()
{
  postGuiEventMessage(GUIEV_LVGL_CAPTURE, 0, NULL, NULL);
}

void app_pairing_open()
{
  postGuiEventMessage(GUIEV_PAIRING_OPEN, 0, NULL, NULL);
}

void app_pairing_close()
{
  postGuiEventMessage(GUIEV_PAIRING_CLOSE, 0, NULL, NULL);
}

AUDIO_CONF *get_audio_conf()
{
  return &AudioConfiguration;
}
