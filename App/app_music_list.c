/**
 * Music Player GUI
 */
#include <stdio.h>
#include <stdlib.h>
#include "DoomPlayer.h"
#include "lvgl.h"
#include "fatfs.h"
#include "app_music.h"
#include "app_task.h"
//#include "cover_if.h"
#include "SDL_mixer.h"
#include "m_misc.h"

#define	MAX_MUSIC	70

extern void SHA1_ComputeDigest(uint8_t *digest, uint8_t *data, int data_len);

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


LV_IMG_DECLARE(img_lv_demo_music_btn_list_play);
LV_IMG_DECLARE(img_lv_demo_music_btn_list_pause);

static inline int IsMusicLump(char *mush)
{
  if ((strncmp(mush, "MUS\032", 4) == 0) ||
      (strncmp(mush, "MThd", 4) == 0))
    return 1;
  return 0;
}

extern char *GetSubstituteMusicFile(void *data, size_t data_len);

typedef struct {
  uint8_t btype;
  uint8_t lenb[3];
} METABH;

typedef struct {
  uint16_t min_block;
  uint16_t max_block;
  uint8_t  min_frame_size[3];
  uint8_t  max_frame_size[3];
  uint8_t  sampleInfo[8];
} STREAM_INFO;

static int play_frequency;
static lv_obj_t *list;

SECTION_DTCMRAM MUSIC_INFO *MusicInfo;

lv_obj_t *getMusicList()
{
  return list;
}

static void collectInfo(MUSIC_INFO *mi)
{
  FIL *fp;
  int newpos;
  uint8_t type;
  METABH metab_header;
  int mblen;
  UINT nr;
  uint32_t *wp;
  char cbuff[20];

  fp = OpenFATFile(mi->path);

  if (fp == NULL)
  {
    debug_printf("failed to open %s\n", mi->path);
    return;
  }

  newpos = 4;		/* Skip Flac signature */

  int blen, clen;
  uint8_t *mp, *memp;
  STREAM_INFO stinfo;

  do
  {
    f_lseek(fp, newpos);
    f_read(fp, &metab_header, sizeof(METABH), &nr);
    type = metab_header.btype;
    mblen = metab_header.lenb[0] << 16 | metab_header.lenb[1] << 8 | metab_header.lenb[2];

    switch (type & 0x7f)
    {
    case 0:		/* STREAMINFI */
      f_read(fp, &stinfo, sizeof(STREAM_INFO), &nr);
      blen = (stinfo.sampleInfo[4] << 24) |
             (stinfo.sampleInfo[5] << 16) |
             (stinfo.sampleInfo[6] << 8) |
             stinfo.sampleInfo[7];
      mi->samples = blen;
      break;
    case 4:  		/* VORBIS_COMMENT */
      blen = mblen;
      memp = mp = malloc(blen);

      f_read(fp, mp, blen, &nr);
      wp = (uint32_t *)mp;
     
      mp += 4 + wp[0];		/* Skip vendor comment */
      blen -= 4 + wp[0];
      wp = (uint32_t *)mp;

      mp += 4;
      blen -= 4;

      while (blen > 0)
      {
        wp = (uint32_t *)mp;
        clen = *wp;
        mp += 4;
        blen -= 4;
        if (memcmp(mp, "TITLE=", 6) == 0)
        {
          mi->title = malloc(clen + 1 - 6);
          memcpy(mi->title, mp + 6, clen - 6);
          mi->title[clen-6] = 0;
        }
        else if (memcmp(mp, "TRACKNUMBER=", 12) == 0)
        {
          memcpy(cbuff, mp, clen);
          cbuff[clen] = 0;
          mi->track = atoi(cbuff + 12);
        }
        else if (memcmp(mp, "ARTIST=", 7) == 0)
        {
          mi->artist = malloc(clen + 1 - 7);
          memcpy(mi->artist, mp + 7, clen - 7);
          mi->artist[clen-7] = 0;
        }
        else if (memcmp(mp, "ALBUM=", 6) == 0)
        {
          mi->album = malloc(clen + 1 - 6);
          memcpy(mi->album, mp + 6, clen - 6);
          mi->album[clen-6] = 0;
        }
        mp += clen;
        blen -= clen;
      }
      free(memp);
      break;
    default:
      break;
    }

    newpos = newpos + sizeof(METABH) + mblen;
  } while (!(metab_header.btype & 0x80));
 
  CloseFATFile(fp);
  osDelay(5);
}

static int list_add(char *fname)
{
  int i;
  MUSIC_INFO *mi = MusicInfo;

  fname = M_StringJoin(GAME_DIR, "/", fname, NULL);

  for (i = 0; i < MAX_MUSIC; i++)
  {
    if (mi->path == NULL)
    {
      mi->path = fname;
      collectInfo(mi);
      return i;
    }
    if (mi->path == fname)
      return 0;
    if (strcmp(mi->path, fname) == 0)
      return 0;
    mi++;
  }
  return 0;
}

static int track_comp(const void *a, const void *b)
{
  MUSIC_INFO *ma, *mb;

  ma = (MUSIC_INFO *)a;
  mb = (MUSIC_INFO *)b;

  return (ma->track - mb->track);
}

static int track_count;

static const lv_font_t * font_small;
static const lv_font_t * font_medium;
static lv_style_t style_scrollbar;
static lv_style_t style_btn;
static lv_style_t style_btn_pr;
static lv_style_t style_btn_chk;
static lv_style_t style_btn_fcs;
static lv_style_t style_btn_dis;
static lv_style_t style_title;
static lv_style_t style_artist;
static lv_style_t style_time;

void _lv_demo_music_list_btn_check(lv_obj_t *list, uint32_t track_id, bool state)
{
    lv_obj_t * btn = lv_obj_get_child(list, track_id);
    lv_obj_t * icon = lv_obj_get_child(btn, 0);

    if(state) {
        lv_obj_add_state(btn, LV_STATE_CHECKED);
        lv_img_set_src(icon, &img_lv_demo_music_btn_list_pause);
        lv_obj_scroll_to_view(btn, LV_ANIM_ON);
        lv_gridnav_set_focused(list, btn, LV_ANIM_OFF);
    }
    else {
        lv_obj_clear_state(btn, LV_STATE_CHECKED);
        lv_img_set_src(icon, &img_lv_demo_music_btn_list_play);
    }
}

static void btn_click_event_cb(lv_event_t * e)
{
    lv_obj_t * btn = lv_event_get_target(e);

    uint32_t idx = lv_obj_get_child_id(btn);

    _lv_demo_music_play(idx);
}

static lv_obj_t *add_list_btn(lv_obj_t *parent, MUSIC_INFO *mi)
{
    lv_obj_t * btn = lv_obj_create(parent);
    lv_obj_remove_style_all(btn);
#if LV_DEMO_MUSIC_LARGE
    lv_obj_set_size(btn, lv_pct(100), 110);
#else
    lv_obj_set_size(btn, lv_pct(100), 60);
#endif

    lv_obj_add_style(btn, &style_btn, 0);
    lv_obj_add_style(btn, &style_btn_pr, LV_STATE_PRESSED);
    lv_obj_add_style(btn, &style_btn_chk, LV_STATE_CHECKED);
    lv_obj_add_style(btn, &style_btn_fcs, LV_STATE_FOCUSED);
    lv_obj_add_style(btn, &style_btn_dis, LV_STATE_DISABLED);
    lv_obj_add_event_cb(btn, btn_click_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * icon = lv_img_create(btn);
    lv_img_set_src(icon, &img_lv_demo_music_btn_list_play);
    lv_obj_set_grid_cell(icon, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, 0, 2);

    lv_obj_t * title_label = lv_label_create(btn);
    lv_label_set_text(title_label, mi->title);
    lv_obj_set_grid_cell(title_label, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    lv_obj_add_style(title_label, &style_title, 0);

    lv_obj_t * artist_label = lv_label_create(btn);
    lv_label_set_text(artist_label, mi->artist);
    lv_obj_add_style(artist_label, &style_artist, 0);
    lv_obj_set_grid_cell(artist_label, LV_GRID_ALIGN_START, 1, 1, LV_GRID_ALIGN_CENTER, 1, 1);

#if 0
    lv_obj_t * time_label = lv_label_create(btn);
    lv_label_set_text(time_label, time);
    lv_obj_add_style(time_label, &style_time, 0);
    lv_obj_set_grid_cell(time_label, LV_GRID_ALIGN_END, 2, 1, LV_GRID_ALIGN_CENTER, 0, 2);
#endif

    LV_IMG_DECLARE(img_lv_demo_music_list_border);
    lv_obj_t * border = lv_img_create(btn);
    lv_img_set_src(border, &img_lv_demo_music_list_border);
    lv_obj_set_width(border, lv_pct(120));
    lv_obj_align(border, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(border, LV_OBJ_FLAG_IGNORE_LAYOUT);

    return btn;
}

lv_obj_t *music_list_create(lv_obj_t *parent, uint32_t dhpos, LUMP_HEADER *lh, int numlumps)
{
  int i;
  char *mush;
  int mindex;
  char *fname;
  MUSIC_INFO *mi;
  lv_obj_t *list;
  lv_group_t *g;

  g = lv_group_create();
  MusicInfo = malloc(MAX_MUSIC * sizeof(MUSIC_INFO));

  memset(MusicInfo, 0, sizeof(MUSIC_INFO) * MAX_MUSIC);
  mi = MusicInfo;
  for (mindex = 0; mindex < MAX_MUSIC; mindex++)
  {
    mi->track = 200;
    mi->title = NULL;
    mi++;
  }

  /* Collect Music information */
  for (i = 0; i < numlumps; i++)
  {
    mush = (char *)dhpos + lh->fpos;
    if (IsMusicLump(mush))
    {
      fname = GetSubstituteMusicFile(mush, lh->fsize);
      list_add(fname);
    }
    lh++;
  }

  qsort(MusicInfo, MAX_MUSIC, sizeof(MUSIC_INFO), track_comp);

#if LV_DEMO_MUSIC_LARGE
  font_small = &lv_font_montserrat_16;
  font_medium = &lv_font_montserrat_22;
#else
  font_small = &lv_font_montserrat_12;
  font_medium = &lv_font_montserrat_16;
#endif

  lv_style_init(&style_scrollbar);
  lv_style_set_width(&style_scrollbar,  4);
  lv_style_set_bg_opa(&style_scrollbar, LV_OPA_COVER);
  lv_style_set_bg_color(&style_scrollbar, lv_color_hex3(0xeee));
  lv_style_set_radius(&style_scrollbar, LV_RADIUS_CIRCLE);
  lv_style_set_pad_right(&style_scrollbar, 4);

    static const lv_coord_t grid_cols[] = {LV_GRID_CONTENT, LV_GRID_FR(1), LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
#if LV_DEMO_MUSIC_LARGE
    static const lv_coord_t grid_rows[] = {35,  30, LV_GRID_TEMPLATE_LAST};
#else
    static const lv_coord_t grid_rows[] = {22,  17, LV_GRID_TEMPLATE_LAST};
#endif
    lv_style_init(&style_btn);
#if 0
    lv_style_set_bg_opa(&style_btn, LV_OPA_TRANSP);
#else
    lv_style_set_bg_opa(&style_btn, LV_OPA_COVER);
    lv_style_set_bg_color(&style_btn, lv_color_hex3(0x101));
#endif
    lv_style_set_grid_column_dsc_array(&style_btn, grid_cols);
    lv_style_set_grid_row_dsc_array(&style_btn, grid_rows);
    lv_style_set_grid_row_align(&style_btn, LV_GRID_ALIGN_CENTER);
    lv_style_set_layout(&style_btn, LV_LAYOUT_GRID);
#if LV_DEMO_MUSIC_LARGE
    lv_style_set_pad_right(&style_btn, 30);
#else
    lv_style_set_pad_right(&style_btn, 20);
#endif
    lv_style_init(&style_btn_pr);
    lv_style_set_bg_opa(&style_btn_pr, LV_OPA_COVER);
    lv_style_set_bg_color(&style_btn_pr,  lv_color_hex(0x4c4965));

    lv_style_init(&style_btn_chk);
    lv_style_set_bg_opa(&style_btn_chk, LV_OPA_COVER);
    lv_style_set_bg_color(&style_btn_chk, lv_color_hex(0x4c4965));

    lv_style_init(&style_btn_fcs);
    lv_style_set_bg_opa(&style_btn_fcs, LV_OPA_COVER);
    lv_style_set_bg_color(&style_btn_fcs, lv_color_hex(0x4c4965));

    lv_style_init(&style_btn_dis);
    lv_style_set_text_opa(&style_btn_dis, LV_OPA_40);
    lv_style_set_img_opa(&style_btn_dis, LV_OPA_40);

    lv_style_init(&style_title);
    lv_style_set_text_font(&style_title, font_medium);
    lv_style_set_text_color(&style_title, lv_color_hex(0xffffff));

    lv_style_init(&style_artist);
    lv_style_set_text_font(&style_artist, font_small);
    lv_style_set_text_color(&style_artist, lv_color_hex(0xb1b0be));

    lv_style_init(&style_time);
    lv_style_set_text_font(&style_time, font_medium);
    lv_style_set_text_color(&style_time, lv_color_hex(0xffffff));

    /*Create an empty transparent container*/
    list = lv_obj_create(parent);
    lv_obj_remove_style_all(list);
    lv_obj_set_size(list, LV_HOR_RES, LV_VER_RES - LV_DEMO_MUSIC_HANDLE_SIZE);
    lv_obj_set_y(list, LV_DEMO_MUSIC_HANDLE_SIZE);
    lv_obj_add_style(list, &style_scrollbar, LV_PART_SCROLLBAR);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);

    lv_gridnav_add(list, LV_GRIDNAV_CTRL_ROLLOVER);
    lv_group_add_obj(g, list);

  track_count = 0;

  for (mi = MusicInfo; mi->title; mi++)
  {
    //debug_printf("%d: %s, %s\n", mi->track, mi->title, mi->path);
    add_list_btn(list, mi);
    track_count++;
  }

#if LV_DEMO_MUSIC_SQUARE || LV_DEMO_MUSIC_ROUND
    lv_obj_set_scroll_snap_y(list, LV_SCROLL_SNAP_CENTER);
#endif

    _lv_demo_music_list_btn_check(list, 0, true);

    MusicInfo->list_group = g;

    return list;
}

void music_process_stick(int evcode)
{
  switch (evcode)
  {
  case GUIEV_YDIR_INC:
    lv_obj_scroll_to_y(MusicInfo->main_cont, LV_VER_RES + LV_DEMO_MUSIC_HANDLE_SIZE, LV_ANIM_ON);
    break;
  case GUIEV_YDIR_DEC:
    lv_obj_scroll_to_y(MusicInfo->main_cont, 0, LV_ANIM_ON);
    break;
  }
}

/**
 * When player container scroll has finished, this callback is called.
 */
static void scroll_cb(lv_event_t *event)
{
  MUSIC_INFO *mi;
  int yoff;

  mi = lv_event_get_user_data(event);

  yoff = lv_obj_get_scroll_y(mi->main_cont);
  if (yoff == 0)
  {
    /* Player screen has been restored. Switch back to main input group. */
    lv_indev_set_group(mi->kdev, mi->main_group);
  }
  else if (yoff >= (LCD_HEIGHT - (LV_DEMO_MUSIC_HANDLE_SIZE * 1)))
  {
    /*
     * Music list screen has been opened. Switch input group to the list.
     */
    lv_indev_set_group(mi->kdev, mi->list_group);
  }
}

lv_obj_t *music_player_create(AUDIO_CONF *audio_config, lv_group_t *g, lv_style_t *btn_style, lv_indev_t *keypad_dev)
{
  QSPI_DIRHEADER *dirInfo = (QSPI_DIRHEADER *)QSPI_ADDR;
  FS_DIRENT *dirent;
  LUMP_HEADER *lh;
  IWAD_HEADER *dh;
  lv_obj_t *scr;

  LoadMusicConfigs();

  play_frequency = audio_config->pseudoRate;
debug_printf("play freq = %d\n", play_frequency);

  dirent = dirInfo->fs_direntry;
  dh  = (IWAD_HEADER *)(QSPI_ADDR + dirent->foffset);

  dirent = dirInfo->fs_direntry;
  lh = (LUMP_HEADER *)(QSPI_ADDR + dirent->foffset + dh->infotab_offset);

  register_cover_file(dirent);

  scr = lv_obj_create(NULL);
  lv_scr_load(scr);
  list = music_list_create(scr, (uint32_t)dh, lh, dh->numlumps);
  MusicInfo->main_cont =  _lv_demo_music_main_create(scr, g, btn_style);
  MusicInfo->main_group = g;
  MusicInfo->kdev = keypad_dev;

  lv_obj_add_event_cb(MusicInfo->main_cont, scroll_cb, LV_EVENT_SCROLL_END, MusicInfo);

  return scr;
}

uint32_t _lv_demo_music_get_track_length(uint32_t track)
{
  return MusicInfo[track].samples / play_frequency;
}

uint32_t _lv_demo_music_get_track_count()
{
  return track_count;
}

const char *_lv_demo_music_get_title(uint32_t track)
{
  return MusicInfo[track].title;
}

const char *_lv_demo_music_get_artist(uint32_t track)
{
  return MusicInfo[track].artist;
}

const char *_lv_demo_music_get_path(uint32_t track)
{
  return MusicInfo[track].path;
}
