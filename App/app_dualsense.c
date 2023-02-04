/**
 * @file app_dualsense.c
 *
 * @brief DualSense controller support
 */
#include <stdio.h>
#include <stdlib.h>
#include "DoomPlayer.h"
#include "lvgl.h"
#include "dualsense_report.h"
#include "app_task.h"
#include "app_gui.h"

static lv_obj_t *ButtonLeds[DS_NUM_BUTTONS];

static lv_img_dsc_t imgdesc;
static struct dualsense_input_report test_report;

static uint32_t find_flash_file(char *name, int *psize)
{
  int i;
  FS_DIRENT *dirent;
  QSPI_DIRHEADER *dirInfo = (QSPI_DIRHEADER *)QSPI_ADDR;

  dirent = dirInfo->fs_direntry;

  for (i = 1; i < NUM_DIRENT; i++)
  {
    if (dirent->foffset == 0xFFFFFFFF)
      return 0; 
    if (strncasecmp(dirent->fname, name, strlen(name)) == 0)
    {
      *psize = dirent->fsize;
      return (uint32_t)(QSPI_ADDR + dirent->foffset);
    }
    dirent++;
  }
  return 0;
}

void Display_DualSense_Info(struct dualsense_input_report *rp, uint8_t hat, uint32_t vbutton)
{
  GUI_EVENT ev;

  memcpy(&test_report, rp, sizeof(test_report));

  ev.evcode = GUIEV_DUALTEST_UPDATE;
  ev.evval0 = vbutton;
  ev.evarg1 = &test_report;
  ev.evarg2 = NULL;
  postGuiEvent(&ev);
}

/* LED object for two pad touch positions */
static lv_obj_t *pad1;
static lv_obj_t *pad2;

static lv_obj_t *img;		/* DualSense photo image */
static int test_active;

/* Variables for Analog stick */
static lv_obj_t *joyleft;
static lv_obj_t *joyright;
static lv_point_t points_left[2] =  { {JOY_RAD, JOY_RAD}, {JOY_RAD, JOY_RAD} };
static lv_point_t points_right[2] = { {JOY_RAD, JOY_RAD}, {JOY_RAD, JOY_RAD} };
static lv_style_t stick_style;

static lv_point_t points_pitch[2] = { {0, 50}, { 100, 50}};

static lv_obj_t *yaw;
static lv_obj_t *pitch;
static lv_obj_t *roll_bar;

void dualtest_done()
{
  GUI_EVENT ev;

  test_active = 0;

  ev.evcode = GUIEV_DUALTEST_DONE;
  ev.evval0 = 0;
  ev.evarg1 = NULL;
  ev.evarg2 = NULL;
  postGuiEvent(&ev);
}

lv_obj_t *dualtest_create()
{
  lv_obj_t *scr;
  int fsize;
  uint32_t faddr;
  int i;
  const GUI_LAYOUT *layout = &GuiLayout;

  scr = lv_obj_create(NULL);
  //lv_obj_set_size(scr, 500, 400);
  lv_obj_set_size(scr, LCD_WIDTH, LCD_HEIGHT);

  /* Load dualsense photo image */
  img = lv_img_create(scr);
  faddr = find_flash_file("dualsense.ibin", &fsize);
  if (faddr)
  {
      imgdesc.header.always_zero = 0;
      imgdesc.header.w = 363;
      imgdesc.header.h = 272;
      imgdesc.header.cf = LV_IMG_CF_RGB565A8;
      imgdesc.data_size = fsize - 4;
      imgdesc.data = (const uint8_t *)(faddr + 4);
      lv_img_set_src(img, &imgdesc);
      lv_img_set_zoom(img, ZOOM_BASE);
      if (ZOOM_BASE != LV_IMG_ZOOM_NONE)
      {
        lv_obj_align(img, LV_ALIGN_LEFT_MID, 100, 0);
      }
  }

  pad1 = lv_led_create(scr);
  lv_obj_set_size(pad1, layout->led_rad, layout->led_rad);
  lv_obj_add_flag(pad1, LV_OBJ_FLAG_HIDDEN);

  pad2 = lv_led_create(scr);
  lv_obj_set_size(pad2, layout->led_rad, layout->led_rad);
  lv_obj_add_flag(pad2, LV_OBJ_FLAG_HIDDEN);

  /* Create LED widgets for all available buttons */
  const lv_point_t *ppos = ButtonPositions;

  for (i = 0; i < DS_NUM_BUTTONS; i++)
  {
     lv_obj_t *pled;

     pled = lv_led_create(scr);
     lv_obj_set_size(pled, layout->led_rad, layout->led_rad);
     lv_obj_add_flag(pled, LV_OBJ_FLAG_HIDDEN);
     lv_led_set_color(pled, lv_palette_main(LV_PALETTE_RED));
     lv_obj_align(pled, LV_ALIGN_TOP_LEFT, ppos->x, ppos->y);

     ButtonLeds[i] = pled;
     ppos++;
  }

  lv_style_init(&stick_style);
  lv_style_set_line_width(&stick_style, layout->line_width);
  lv_style_set_line_color(&stick_style, lv_palette_main(LV_PALETTE_BLUE));

  joyleft = lv_line_create(scr);
  lv_obj_set_size(joyleft, JOY_DIA, JOY_DIA);
  lv_obj_add_style(joyleft, &stick_style, 0);
  lv_obj_align(joyleft, LV_ALIGN_TOP_LEFT, layout->joyl_x - JOY_RAD, layout->joy_y);
  joyright = lv_line_create(scr);
  lv_obj_set_size(joyright, JOY_DIA, JOY_DIA);
  lv_obj_add_style(joyright, &stick_style, 0);
  lv_obj_align(joyright, LV_ALIGN_TOP_LEFT, layout->joyr_x - JOY_RAD, layout->joy_y);

  roll_bar = lv_bar_create(scr);
  lv_obj_set_size(roll_bar, layout->bar_width, layout->bar_height);
  lv_obj_align(roll_bar, LV_ALIGN_BOTTOM_RIGHT, layout->bar_xpos, layout->bar_ypos);
  lv_bar_set_mode(roll_bar, LV_BAR_MODE_RANGE);
  lv_bar_set_start_value(roll_bar, 50, LV_ANIM_OFF);
  lv_bar_set_value(roll_bar, 30, LV_ANIM_OFF);
  lv_bar_set_range(roll_bar, 0, 100);

  pitch = lv_line_create(scr);
  lv_obj_set_size(pitch, layout->bar_height, layout->bar_height);
  lv_obj_add_style(pitch, &stick_style, 0);
  lv_obj_align(pitch, LV_ALIGN_BOTTOM_RIGHT, -8, layout->bar_ypos);

  yaw = lv_arc_create(scr);
  lv_obj_set_size(yaw, layout->bar_height, layout->bar_height);
  lv_obj_clear_flag(yaw, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_align(yaw, LV_ALIGN_BOTTOM_RIGHT, -8, layout->bar_ypos);
  lv_arc_set_bg_angles(yaw, 0, 360);
  lv_arc_set_mode(yaw, LV_ARC_MODE_NORMAL);
  lv_arc_set_angles(yaw, 0, 0);
lv_obj_set_style_arc_width(yaw, layout->line_width, LV_PART_MAIN);
lv_obj_set_style_arc_width(yaw, layout->line_width * 2, LV_PART_INDICATOR);
lv_obj_set_style_pad_top(yaw, 0, LV_PART_KNOB);
lv_obj_set_style_pad_bottom(yaw, 0, LV_PART_KNOB);
lv_obj_set_style_pad_left(yaw, 0, LV_PART_KNOB);
lv_obj_set_style_pad_right(yaw, 0, LV_PART_KNOB);

  lv_obj_t *home, *label;

  home = lv_btn_create(scr);
  lv_obj_set_size(home, W_PERCENT(21), H_PERCENT(12));
  lv_obj_align(home, LV_ALIGN_BOTTOM_RIGHT, -8, -20);
  lv_obj_add_event_cb(home, dualtest_done, LV_EVENT_CLICKED, NULL);
  lv_obj_set_style_bg_color(home, lv_palette_main(LV_PALETTE_ORANGE), 0);
  label = lv_label_create(home);
  lv_label_set_text(label, LV_SYMBOL_HOME);
  lv_obj_center(label);

  test_active = 1;
  return scr;
}

/*
 *  Pad area on the dualsense image bitmap
 */

#define	TP_XSCALE(l, x)	(x * l->padi_width / DS_TOUCHPAD_WIDTH)
#define	TP_YSCALE(l, y)	(y * l->padi_height / DS_TOUCHPAD_HEIGHT)

void dualtest_update(struct dualsense_input_report *rp, uint32_t vbutton)
{
  const GUI_LAYOUT *layout = &GuiLayout;

  struct dualsense_touch_point *tp;
  int xpos, ypos;
  int wx, wy;
  int i;
  int mask = 1;
  int16_t angle;
  lv_obj_t *led;

  /* DualSense test mode */

  /* Draw Button LED status */

  for ( i = 0; i < DS_NUM_BUTTONS; i++)
  {
    led = ButtonLeds[i];
    int size;

    if (vbutton & mask)
    {
      lv_obj_clear_flag(led, LV_OBJ_FLAG_HIDDEN);
      if (i == 6)
      {
        size = rp->z / 10;
        if (size < 3) size = 3;
        lv_obj_set_size(led, size, size);
      }
      else if (i == 7)
      {
        size = rp->rz / 10;
        if (size < 3) size = 3;
        lv_obj_set_size(led, size, size);
      }
    }
    else
    {
      lv_obj_add_flag(led, LV_OBJ_FLAG_HIDDEN);
    }
    mask <<= 1;
  }

  /* Draw toch pad position */

  tp = &rp->points[0];		// first point
  if (tp->contact & 0x80)
  {
    lv_obj_add_flag(pad1, LV_OBJ_FLAG_HIDDEN);
  }
  else
  {
    wx = ((tp->x_hi << 8) | tp->x_lo);
    wy = ((tp->y_hi << 4) | tp->y_lo);
    xpos = TP_XSCALE(layout, wx) + layout->padi_xpos;	// PADI_XPOS;
    ypos = TP_YSCALE(layout, wy) + layout->padi_ypos;	// PADI_YPOS;
    lv_obj_align(pad1, LV_ALIGN_TOP_LEFT, xpos, ypos);
    lv_obj_clear_flag(pad1, LV_OBJ_FLAG_HIDDEN);
  }
  tp = &rp->points[1];		// second point
  if (tp->contact & 0x80)
  {
    lv_obj_add_flag(pad2, LV_OBJ_FLAG_HIDDEN);
  }
  else
  {
    wx = ((tp->x_hi << 8) | tp->x_lo);
    wy = ((tp->y_hi << 4) | tp->y_lo);
    xpos = TP_XSCALE(layout, wx) + layout->padi_xpos;	// PADI_XPOS;
    ypos = TP_YSCALE(layout, wy) + layout->padi_ypos;	// PADI_YPOS;
    lv_obj_align(pad2, LV_ALIGN_TOP_LEFT, xpos, ypos);
    lv_obj_clear_flag(pad2, LV_OBJ_FLAG_HIDDEN);
  }

  /* Draw left and right joystick bars */

// debug_printf("(%d, %d, %d)\n", rp->x, rp->y, rp->z);  // 0..256
  points_left[1].x = rp->x / layout->joy_divisor;
  points_left[1].y = rp->y / layout->joy_divisor;
  lv_line_set_points(joyleft, points_left, 2);
  points_right[1].x = rp->rx / layout->joy_divisor;
  points_right[1].y = rp->ry / layout->joy_divisor;
  lv_line_set_points(joyright, points_right, 2);

  /* Draw IMU angles */

  angle = ImuAngle.pitch;
  points_pitch[0].x = (layout->bar_height/2) + ((layout->bar_height/2) * lv_trigo_cos(angle))/32768;
  points_pitch[0].y = (layout->bar_height/2) + ((layout->bar_height/2) * lv_trigo_sin(angle))/32768;
  points_pitch[1].x = (layout->bar_height/2) - ((layout->bar_height/2) * lv_trigo_cos(angle))/32768;
  points_pitch[1].y = (layout->bar_height/2) - ((layout->bar_height/2) * lv_trigo_sin(angle))/32768;
  lv_line_set_points(pitch, points_pitch, 2);

  angle = ImuAngle.roll;
  if (angle > 90)
    angle = 180 - angle;
  if (angle < -90)
    angle = -180 - angle;

  int val;

  val = 50 + (50 * lv_trigo_sin(angle)/32768);
  if (val >= 50)
  {
    lv_bar_set_start_value(roll_bar, 50, LV_ANIM_OFF);
    lv_bar_set_value(roll_bar, val, LV_ANIM_OFF);
  }
  else
  {
    lv_bar_set_start_value(roll_bar, val, LV_ANIM_OFF);
    lv_bar_set_value(roll_bar, 50, LV_ANIM_OFF);
  }
  

  angle = 360 - ImuAngle.yaw;
  angle -= 90;
  if (angle < 0)
    angle += 360;

  lv_arc_set_angles(yaw, angle, angle);
}
