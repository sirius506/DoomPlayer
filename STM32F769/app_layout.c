/**
 *  GUI layout parameters for 800x480 32F769DISCOVERY board
 */
#include "DoomPlayer.h"
#include "app_gui.h"

const GUI_LAYOUT GuiLayout = {
  .font_title = &lv_font_montserrat_40,
  .font_small = &lv_font_montserrat_22,
  .font_large = &lv_font_montserrat_32,
#if 0
  .font_h1 = &lv_font_montserrat_20,
  .font_h2 = LV_FONT_DEFAULT,
#endif

  .spinner_width = 20,

  /* Menu screen */

  .mb_yoffset = 135,
  .mb_height = 95,
  .mb_olw = 8,

  .audio_box_width = 200,
  .audio_box_height = 145,
  .audio_box_yoffset = 120,
  .audio_button_height = 60,

  /* DualSense demo screen */

  .led_rad = 13,
  .joyl_x = 195,
  .joyr_x = 365,
  .joy_y = 320,
  .joy_divisor = 4,

  .line_width = 10,

  .bar_width = 20,
  .bar_height = 168,
  .bar_xpos = -84,
  .bar_ypos =  -105,

  .padi_xpos = 190,
  .padi_ypos = 80,
  .padi_width = 180,
  .padi_height = 95,
};

const lv_point_t ButtonPositions[DS_NUM_BUTTONS] = {
  { 405, 170 }, /* Square */
  { 440, 220 }, /* Cross */
  { 480, 170 }, /* Circle */
  { 440, 125 }, /* Triangle */
  { 115,  70 }, /* L1 */
  { 445,  70 }, /* R1 */
  { 100,  50 },	/* L2 */
  { 450,  50 },	/* R2 */
  { 155, 110 },	/* Create */
  { 405, 110 },	/* Option */
  { 195, 245 },	/* L3 */
  { 365, 245 },	/* R3 */
  { 275, 240 },	/* PS */
  { 275,  80 },	/* Touch */
  { 275, 270 },	/* MUTE */
  { 110, 132 },	/* 15 - Up */
  {  77, 170 }, /* 16 - Left */
  { 145, 170 }, /* 17 - Right */
  { 110, 200 }, /* 18 - Down */
};
