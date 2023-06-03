#ifndef _APP_SOUND_H
#define _APP_SOUND_H
#include "mplayer.h"

#define	PCM_MAGIC	0x0003
#define	PCM_NORMAL_RATE	11025
#define	PCM_DOUBLE_RATE	22050

#define	MAX_SOUNDS	120
#define	SOUND_SAMPLES	512

typedef struct {
  uint16_t magic;
  uint16_t rate;
} PCM_HEADER;

typedef struct {
  char name[9];
  uint16_t length;
  uint16_t rate;
  uint8_t factor;
  uint8_t *pos;
} SOUND_DATA;

typedef struct {
  lv_obj_t *chart;
  lv_obj_t *slider;
  lv_obj_t *title;
  lv_obj_t *play_obj;
  lv_chart_series_t *ser;
  lv_chart_cursor_t *cursor;
  lv_point_t cursor_point;
  lv_coord_t *chart_vars;
  int16_t  posdiv;		// Postion divisor
  int16_t  ticks;
} CHART_INFO;


extern void app_sound_end();
lv_obj_t *sound_screen_create(lv_obj_t *parent, lv_group_t *ing, lv_style_t *btn_style);
#endif
