/**
 * @file app_sound.c
 *
 * @brief Doom Player Sound screen handler.
 */
#include "DoomPlayer.h"
#include <stdlib.h>
#include <stdio.h>
#include "lvgl.h"
#include "app_task.h"
#include "app_sound.h"
#include "arm_math.h"
#include "app_gui.h"

SECTION_DTCMRAM static CHART_INFO ChartInfo;
static Mix_Chunk sound_chunk;

typedef struct {
  char     idstring[4];
  uint32_t numlumps;
  uint32_t infotab_offset;
} IWAD_HEADER;

typedef struct {
  uint32_t fpos;
  uint32_t fsize;
  char     lumpname[8];
} LUMP_HEADER;

SOUND_DATA SoundList[MAX_SOUNDS];
static SOUND_DATA *sdpCurrent;

static lv_timer_t *cursor_timer;

static void app_pcm_set(SOUND_DATA *sdp);

/**
 * @brief Load PCM sound data into dynamicaly allocated buffer
 */
void set_sound_chart(CHART_INFO *chartInfo, SOUND_DATA *sdp)
{
  int i;
  int16_t v;
  lv_coord_t *dp;
  uint8_t *pcm;

  if (chartInfo->chart_vars) 
    free(chartInfo->chart_vars); /* Frees exisiting buffer space. */

  /* If PCM sound data size is greater than 32K, then we'll decimate
   * the chart data to the half size.
   */
  chartInfo->posdiv = (sdp->length > 32767)? 2 : 1;
  dp = chartInfo->chart_vars = malloc(sizeof(lv_coord_t) * sdp->length / chartInfo->posdiv);
  pcm = sdp->pos;

  /* WAD PCM data is 8bit, but we need 16bit signed value for chart widget.  */

  for (i = 0; i < sdp->length; i++)
  {
    v = (*pcm++ ^ 0x80) << 8;
    *dp++ = v;
    if (chartInfo->posdiv > 1)	/* Need to decimate? */
    {
      i++;
      pcm++;	/* skip a sample */
    }  
  }
  lv_chart_set_point_count(chartInfo->chart, sdp->length / chartInfo->posdiv);
  lv_chart_set_ext_y_array(chartInfo->chart, chartInfo->ser, chartInfo->chart_vars);

  app_pcm_set(sdp);		/* Set stereo PCM data */

  lv_imgbtn_set_state(chartInfo->play_obj, LV_IMGBTN_STATE_RELEASED);

  chartInfo->ticks = 0;
  lv_timer_reset(cursor_timer);
  lv_timer_pause(cursor_timer);

  lv_slider_set_value(chartInfo->slider, LV_IMG_ZOOM_NONE, LV_ANIM_OFF);
  lv_chart_set_zoom_x(chartInfo->chart, LV_IMG_ZOOM_NONE);

  lv_chart_get_point_pos_by_id(chartInfo->chart, chartInfo->ser, 0, &chartInfo->cursor_point);
  lv_chart_set_cursor_pos(chartInfo->chart, chartInfo->cursor, &chartInfo->cursor_point);
}

void set_sound_title(lv_obj_t *title, SOUND_DATA *sdp)
{
  char tbuffer[40];
  int psec;

  psec = sdp->length * 100 / sdp->rate;
  snprintf(tbuffer, sizeof(tbuffer), "%s (%dHz, %d.%02dsec)",
     sdp->name, sdp->rate, psec / 100, psec % 100);
  lv_label_set_text(title, tbuffer);
}

/**
 * @brief Called when music list button is pressed.
 */
static void event_handler(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);
  //lv_obj_t *obj = lv_event_get_target(e);

  if (code == LV_EVENT_CLICKED)
  {
    int index = (int)lv_event_get_user_data(e);
    SOUND_DATA *sdp;

    sdp = &SoundList[index];
    if (sdp != sdpCurrent)	/* Load newly selected sound. */
    {
      CHART_INFO *chartInfo = &ChartInfo;

      sdpCurrent = sdp;
      set_sound_title(chartInfo->title, sdpCurrent);
      set_sound_chart(chartInfo, sdpCurrent);
    }
  }
}

static int soundCompare(const void *p1, const void *p2)
{
  SOUND_DATA *s1 = (SOUND_DATA *)p1;
  SOUND_DATA *s2 = (SOUND_DATA *)p2;

  return(strncmp(s1->name, s2->name, 8));
}

static int sound_exist(SOUND_DATA *top,  char *name)
{
  int i;

  for (i = 0; i < MAX_SOUNDS; i++)
  {
    if (top->name[0] == 0)
    {
      return 0;
    }
    if (strncmp(top->name, name, 8) == 0)
      return 1;
    top++;
  }
  return 0;
}

static void slider_x_event_cb(lv_event_t *e)
{
  lv_obj_t *obj = lv_event_get_target(e);
  CHART_INFO *cinfo = (CHART_INFO *)lv_event_get_user_data(e);
  int32_t v = lv_slider_get_value(obj);
  lv_chart_set_zoom_x(cinfo->chart, v);
}

/*
 * Called when Play/Pause button has pressed
 */
static void play_event_click_cb(lv_event_t * e)
{
  lv_obj_t * obj = lv_event_get_target(e);

  if(lv_obj_has_state(obj, LV_STATE_CHECKED)) {
    Mix_ResumeChannel(0);
    lv_timer_resume(cursor_timer);
  } else {
    lv_timer_pause(cursor_timer);
    Mix_PauseChannel(0);
  }
}

lv_obj_t *sound_list_create(lv_obj_t *parent, LUMP_HEADER *lh, int numlumps, uint32_t dhpos, lv_group_t *ing)
{
    lv_obj_t *list;
    CHART_INFO *chartInfo = &ChartInfo;
    lv_obj_t *btn;

    int i, sindex;
    char mbuffer[50];
    PCM_HEADER *pcm;

    sindex = 0;
    SOUND_DATA *sdp = SoundList;

    sdpCurrent = sdp;


    list = lv_list_create(parent);
    lv_obj_set_size(list, lv_pct(30), lv_pct(76));
    lv_gridnav_add(list, LV_GRIDNAV_CTRL_ROLLOVER);
    lv_group_add_obj(ing, list);
  
    /* Collect sound LUMP information */
  
    for (i = 0; i < numlumps; i++)
    {
      if (strncmp(lh->lumpname, "DS", 2) == 0)
      {
        if (!sound_exist(SoundList, lh->lumpname))
        {

          memcpy(mbuffer, lh->lumpname, 8);
          mbuffer[8] = 0;
          pcm = (PCM_HEADER *)(dhpos + lh->fpos);
          if ((pcm->magic == PCM_MAGIC)  &&
           ((pcm->rate == PCM_NORMAL_RATE) || (pcm->rate == PCM_DOUBLE_RATE)) &&
           (sindex < MAX_SOUNDS))
          {
            memcpy(sdp->name, mbuffer, 9);
            sdp->rate = pcm->rate;
            sdp->factor = (sdp->rate > 11025)? 2 : 4;
            sdp->pos = (uint8_t *)pcm + sizeof(PCM_HEADER) + 8;
            sdp->length = lh->fsize - sizeof(PCM_HEADER) - 16;
            sindex++;
            sdp++;
          }
        }
      }
      lh++;
    }

    qsort((void *)SoundList, sindex, sizeof(SOUND_DATA), soundCompare);

    /* Create sound list buttons */

    sdp = SoundList;
    for (i = 0; i < sindex; i++)
    {
       lv_snprintf(mbuffer, 10, "%s", sdp->name);
       btn = lv_list_add_btn(list, NULL, mbuffer);
       lv_obj_add_event_cb(btn, event_handler, LV_EVENT_CLICKED, (void *)i);
       sdp++;
    }
    lv_obj_align(list, LV_ALIGN_TOP_LEFT, lv_pct(5), lv_pct(12));

    debug_printf("Total %d sounds.\n", sindex);

    chartInfo->title = lv_label_create(parent);
    set_sound_title(chartInfo->title, sdpCurrent);
    lv_obj_align(chartInfo->title, LV_ALIGN_TOP_LEFT, lv_pct(40), lv_pct(10));

    /* Add chart */
    chartInfo->chart = lv_chart_create(parent);
    lv_obj_set_size(chartInfo->chart, lv_pct(58), lv_pct(40));
    lv_obj_align(chartInfo->chart, LV_ALIGN_TOP_LEFT, lv_pct(40), lv_pct(16));
#ifdef USE_ARROW_KEYS
    lv_obj_add_flag(chartInfo->chart, LV_OBJ_FLAG_SCROLL_WITH_ARROW);
    lv_group_add_obj(ing, chartInfo->chart);
#endif

    /* Do not display points on the data */
    lv_obj_set_style_size(chartInfo->chart, 0, LV_PART_INDICATOR);

    lv_chart_set_range(chartInfo->chart, LV_CHART_AXIS_PRIMARY_Y, -32768, 32767);

    chartInfo->cursor  = lv_chart_add_cursor(chartInfo->chart, lv_palette_main(LV_PALETTE_GREEN), LV_DIR_VER);

    chartInfo->ser = lv_chart_add_series(chartInfo->chart, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);

    chartInfo->slider = lv_slider_create(parent);
    lv_slider_set_range(chartInfo->slider, LV_IMG_ZOOM_NONE, LV_IMG_ZOOM_NONE * 10);
    lv_obj_set_size(chartInfo->slider, W_PERCENT(40), H_PERCENT(4));
    lv_obj_align_to(chartInfo->slider, chartInfo->chart, LV_ALIGN_OUT_BOTTOM_MID, 0, H_PERCENT(30));
    lv_obj_add_event_cb(chartInfo->slider, slider_x_event_cb, LV_EVENT_VALUE_CHANGED, chartInfo);
    //lv_group_add_obj(ing, chartInfo->slider);

    LV_IMG_DECLARE(img_lv_demo_music_btn_play);
    LV_IMG_DECLARE(img_lv_demo_music_btn_pause);

    static lv_style_t style_focus;

    lv_style_init(&style_focus);
    lv_style_set_img_recolor_opa(&style_focus, LV_OPA_20);
    lv_style_set_img_recolor(&style_focus, lv_color_black());

    chartInfo->play_obj = lv_imgbtn_create(parent);
    lv_obj_align_to(chartInfo->play_obj, chartInfo->chart, LV_ALIGN_OUT_BOTTOM_MID, 0, H_PERCENT(2));
    lv_imgbtn_set_src(chartInfo->play_obj, LV_IMGBTN_STATE_RELEASED, NULL, &img_lv_demo_music_btn_play, NULL);
    lv_imgbtn_set_src(chartInfo->play_obj, LV_IMGBTN_STATE_CHECKED_RELEASED, NULL, &img_lv_demo_music_btn_pause, NULL);
    lv_obj_add_style(chartInfo->play_obj, &style_focus, LV_STATE_FOCUSED);

    lv_obj_add_flag(chartInfo->play_obj, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_set_width(chartInfo->play_obj, img_lv_demo_music_btn_play.header.w);
    lv_obj_add_event_cb(chartInfo->play_obj, play_event_click_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_flag(chartInfo->play_obj, LV_OBJ_FLAG_CLICKABLE);

    //lv_gridnav_add(chartInfo->play_obj, LV_GRIDNAV_CTRL_NONE);
    lv_group_add_obj(ing, chartInfo->play_obj);
    set_sound_chart(chartInfo, sdpCurrent);
    return list;
}

static void cursor_update_callback(lv_timer_t *t)
{
  CHART_INFO *chartInfo = &ChartInfo;
  uint32_t ppos;

  ppos = Mix_PlayPosition(0);


  if ((ppos == 0) && (chartInfo->ticks > 0))
  {
    /* If playposition is zero, it meaans play has finished.
     * Stop timer and change button image.
     */
    chartInfo->ticks = 0;
    lv_timer_pause(t);
    lv_imgbtn_set_state(chartInfo->play_obj, LV_IMGBTN_STATE_RELEASED);
  }
  else
  {
    chartInfo->ticks++;
    ppos /= sdpCurrent->factor;
    if (ppos > sdpCurrent->length)
      ppos = sdpCurrent->length - 1;
  }
  /* Update cusor position */
  lv_chart_get_point_pos_by_id(chartInfo->chart, chartInfo->ser, ppos / chartInfo->posdiv, &chartInfo->cursor_point);
  lv_chart_set_cursor_pos(chartInfo->chart, chartInfo->cursor, &chartInfo->cursor_point);
}

static void sound_quit(lv_event_t *e)
{
  postGuiEventMessage(GUIEV_MPLAYER_DONE, 0, NULL, NULL);
}

/**
 * Create Sound Player screen
 */
void sound_screen_create(lv_obj_t *parent, lv_group_t *ing, lv_style_t *btn_style)
{
  QSPI_DIRHEADER *dirInfo = (QSPI_DIRHEADER *)QSPI_ADDR;
  FS_DIRENT *dirent;
  lv_obj_t *label, *btn_home;

  dirent = dirInfo->fs_direntry;

  static uint32_t dhpos;

  dhpos = QSPI_ADDR + dirent->foffset;

  IWAD_HEADER *dh = (IWAD_HEADER *)dhpos;
  LUMP_HEADER *lh = (LUMP_HEADER *)(QSPI_ADDR + dirent->foffset + dh->infotab_offset);

  cursor_timer = lv_timer_create(cursor_update_callback, 100, NULL);

  lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

  sound_list_create(parent, lh, dh->numlumps, dhpos, ing);

  /* Add home button */

  btn_home = lv_btn_create(parent);
  lv_obj_add_style(btn_home, btn_style, LV_STATE_FOCUS_KEY|LV_STATE_FOCUS_KEY);
  lv_obj_set_size(btn_home, W_PERCENT(14), W_PERCENT(14));
  lv_obj_align(btn_home, LV_ALIGN_TOP_RIGHT, W_PERCENT(7), -W_PERCENT(7));
  lv_obj_set_style_radius(btn_home, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(btn_home, lv_palette_main(LV_PALETTE_ORANGE), 0);
  lv_obj_add_event_cb(btn_home, sound_quit, LV_EVENT_CLICKED, 0);

  label = lv_label_create(btn_home);
  lv_label_set_text(label, LV_SYMBOL_HOME);
#ifdef OLD_CODE
  lv_obj_align(label, LV_ALIGN_BOTTOM_LEFT, -6, 0);
#else
  //lv_obj_align(label, LV_ALIGN_BOTTOM_LEFT, -2, -9);
  lv_obj_align(label, LV_ALIGN_BOTTOM_LEFT, -2, -5);
#endif

  lv_group_add_obj(ing, btn_home);

  lv_timer_pause(cursor_timer);
}

#define	NUMTAPS	28
#define	BLSIZE	100

static arm_fir_interpolate_instance_q15 FirInstance;
static q15_t FirState[NUMTAPS/2+BLSIZE];

const q15_t Coeffs4[NUMTAPS] = {
  0xffc7, 0xffe3, 0x002c, 0x00ac,
  0x010d, 0x00a7, 0xff0b, 0xfcb7,
  0xfb66, 0xfd55, 0x03d1, 0x0def,
  0x186c, 0x1f1e, 0x1f1e, 0x186c,
  0x0def, 0x03d1, 0xfd55, 0xfb66,
  0xfcb7, 0xff0b, 0x00a7, 0x010d,
  0x00ac, 0x002c, 0xffe3, 0xffc7
};


const q15_t Coeffs2[NUMTAPS] = {
  0x002b, 0x0036, 0xffae, 0xff7d,
  0x00cd, 0x0135, 0xfe3d, 0xfd7f,
  0x0382, 0x04eb, 0xf8fa, 0xf560,
  0x12a1, 0x394d, 0x394d, 0x12a1,
  0xf560, 0xf8fa, 0x04eb, 0x0382,
  0xfd7f, 0xfe3d, 0x0135, 0x00cd,
  0xff7d, 0xffae, 0x0036, 0x002b
};

static int16_t interp_buffer[BLSIZE*4];

static void app_pcm_set(SOUND_DATA *sdp)
{
  int i, nb;
  int16_t *dp, *wp;
  int16_t *stb;
  uint8_t *pcm;
  int16_t v;
  const q15_t *pCoeffs;

  if (sound_chunk.abuf)
  {
    free(sound_chunk.abuf);
  }
  nb = sdp->length * 4;		// 16bit stereo needs 4 bytes
  nb *= sdp->factor;
  sound_chunk.abuf = malloc(nb);
  sound_chunk.alen = nb;
  sound_chunk.volume = 100;


  stb = wp = dp = (int16_t *)malloc(sdp->length * 2);
  pcm = sdp->pos;

  /* WAD PCM data is 8bit, but we need 16bit signed value for further process.  */

  for (i = 0; i < sdp->length; i++)
  {
    v = (*pcm++ ^ 0x80) << 8;
    *dp++ = v;
  }

  /*
   * WAD PCM data is sampled at 11.025K or 22.06KHz.
   * Convert it to 44.1K sampling stereo data.
   */
  pCoeffs = (sdp->factor == 2)? Coeffs2 : Coeffs4;
  arm_fir_interpolate_init_q15(&FirInstance, sdp->factor, NUMTAPS, (q15_t *)pCoeffs, FirState, BLSIZE);

  int len, olen;
  AUDIO_STEREO *pAudio = (AUDIO_STEREO *)sound_chunk.abuf;

  for (len = sdp->length; len > 0; len -= nb)
  {
    nb = (len > BLSIZE)? BLSIZE : len;
    olen = nb * sdp->factor;
    arm_fir_interpolate_q15(&FirInstance, wp, interp_buffer, nb);

    dp = interp_buffer;

    /* Convert to stereo data */
    for (i = 0; i < olen; i++)
    {
      pAudio->ch0 = *dp;
      pAudio->ch1 = *dp;
      pAudio++;
      dp++;
    }
    wp += nb;
  }
  free(stb);
  Mix_LoadChannel(0, &sound_chunk, 1);
}

void sound_process_stick(int evcode)
{
  CHART_INFO *cinfo = &ChartInfo;
  int w, val;

  val = lv_slider_get_value(cinfo->slider);

  switch (evcode)
  {
  case GUIEV_XDIR_INC:
    val = lv_obj_get_scroll_x(cinfo->chart);
    w = lv_obj_get_width(cinfo->chart);
    lv_obj_scroll_to_x(cinfo->chart, val + w / 2, LV_ANIM_OFF);
    break;
  case GUIEV_XDIR_DEC:
    val = lv_obj_get_scroll_x(cinfo->chart);
    w = lv_obj_get_width(cinfo->chart);
    val -= w / 2;
    if (val < 0) val = 0;
    lv_obj_scroll_to_x(cinfo->chart, val, LV_ANIM_OFF);
    break;
  case GUIEV_YDIR_INC:
    if (val < LV_IMG_ZOOM_NONE * 10)
      val += LV_IMG_ZOOM_NONE;
    lv_slider_set_value(cinfo->slider, val, LV_ANIM_OFF);
    lv_chart_set_zoom_x(cinfo->chart, val);
    break;
  case GUIEV_YDIR_DEC:
    if (val > LV_IMG_ZOOM_NONE)
      val -= LV_IMG_ZOOM_NONE;
    lv_slider_set_value(cinfo->slider, val, LV_ANIM_OFF);
    lv_chart_set_zoom_x(cinfo->chart, val);
    break;
  }
}
