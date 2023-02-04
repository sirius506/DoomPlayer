/**
 *  GUI layout parameters for 480x272 STM32H7B3I-DK board
 */
#include "DoomPlayer.h"
#include "app_gui.h"

const GUI_LAYOUT GuiLayout = {
  .font_title = &lv_font_montserrat_20,
  .font_small = &lv_font_montserrat_12,
  .font_large = &lv_font_montserrat_16,
#if 0
  .font_h1 = &lv_font_montserrat_16,
  .font_h2 = LV_FONT_DEFAULT,
#endif

  .spinner_width = 12,

  /* Menu screen */

  .mb_yoffset = 80,
  .mb_height = 50,
  .mb_olw = 4,

  .audio_box_width = 130,
  .audio_box_height = 96,
  .audio_box_yoffset = 70,
  .audio_button_height = 35,

  /* DualSense demo screen */

  .led_rad = 5,
  .joyl_x = 128,
  .joyr_x = 237,
  .joy_y = 205,
  .joy_divisor = 5,

  .line_width = 5,

  .bar_width = 10,
  .bar_height = 100,
  .bar_xpos = -50,
  .bar_ypos =  -85,

  .padi_xpos = 125,
  .padi_ypos = 35,
  .padi_width = 110,
  .padi_height = 60,
};

const lv_point_t ButtonPositions[DS_NUM_BUTTONS] = {
  { 261,  90 }, /* Square */
  { 284, 115 }, /* Cross */
  { 310,  89 }, /* Circle */
  { 284,  65 }, /* Triangle */
  {  80,  34 }, /* L1 */
  { 287,  34 }, /* R1 */
  {  53,  24 },	/* L2 */
  { 297,  24 },	/* R2 */
  { 104,  54 },	/* Create */
  { 261,  54 },	/* Option */
  { 126, 138 },	/* L3 */
  { 237, 138 },	/* R3 */
  { 182, 137 },	/* PS */
  { 182,  25 },	/* Touch */
  { 182, 157 },	/* MUTE */
  {  75,  70 },	/* 15 - Up */
  {  57,  90 }, /* 16 - Left */
  {  94,  90 }, /* 17 - Right */
  {  75, 109 }, /* 18 - Down */
};
