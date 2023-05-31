#ifndef _APP_GUI_H
#define _APP_GUI_H

#include "lvgl.h"

#define	W_PERCENT(x)	(LCD_WIDTH * x / 100)
#define	H_PERCENT(y)	(LCD_HEIGHT * y / 100)

#define	DS_NUM_BUTTONS	19

extern const lv_point_t ButtonPositions[DS_NUM_BUTTONS];

typedef struct {
  const lv_font_t *font_title;
  const lv_font_t *font_small;
  const lv_font_t *font_large;

  int16_t  spinner_width;

  /* Menu screen */
  uint16_t mb_yoffset;
  uint16_t mb_height;
  uint16_t mb_olw;		// Menu butoon outline width

  uint16_t audio_box_width;
  uint16_t audio_box_height;
  uint16_t audio_box_yoffset;
  uint16_t audio_button_height;
  /* DualSense demo screen */
  uint16_t led_rad;		// button led size
  uint16_t joyl_x;		// Left joystick x position
  uint16_t joyr_x;		// Right joystick y position
  uint16_t joy_y;		// Joystick y position
  uint16_t joy_divisor;

  uint16_t line_width;

  uint16_t bar_width;		// Roll bar width
  uint16_t bar_height;		// Roll bar height
  uint16_t bar_xpos;
  uint16_t bar_ypos;

  uint16_t padi_xpos;
  uint16_t padi_ypos;
  uint16_t padi_width;
  uint16_t padi_height;
} GUI_LAYOUT;

typedef struct {
  lv_obj_t *screen;
  lv_obj_t *title;
  lv_obj_t *mbox;	/* Message box object */
  lv_obj_t *btn;
  lv_obj_t *spinner;
  lv_group_t *ing;		// Input group
} START_SCREEN;

typedef struct {
  lv_obj_t *screen;
  lv_obj_t *title;
  lv_obj_t *btn_music;
  lv_obj_t *btn_sound;
  lv_obj_t *btn_dual;
  lv_obj_t *btn_game;
  lv_obj_t *btn_audio;
  lv_obj_t *cont_audio;
  lv_obj_t *cont_bt;
  lv_group_t *ing;		// Input group
  lv_obj_t *sub_scr;
  lv_obj_t *play_scr;
  lv_group_t *player_ing;	// Input group for player
} MENU_SCREEN;

typedef struct {
  lv_obj_t *screen;
  lv_obj_t *title;
  lv_obj_t *operation;
  lv_obj_t *fname;
  lv_obj_t *mbox;	/* Message box object */
  lv_obj_t *bar;
  lv_obj_t *progress;
} COPY_SCREEN;

typedef struct {
  lv_obj_t *screen;
  lv_obj_t *cheat_btn;
  lv_obj_t *kbd;
  lv_obj_t *ta;
  lv_obj_t *cheat_code;
  lv_obj_t *img;
} GAME_SCREEN;

typedef struct {
  lv_obj_t *screen;
  lv_group_t *ing;		// Input group
} SOUND_SCREEN;

extern const GUI_LAYOUT GuiLayout;
extern void send_padkey(lv_indev_data_t *pdata);
extern void app_pairing_open();
extern void app_pairing_close();
#endif
