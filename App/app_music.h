#ifndef _APP_MUSIC_H
#define _APP_MUSIC_H
#include "doomfs.h"

/* Cover file must be ARGB565 binary format and 128x128 pixels */
#define	COVER_MAGIC	0x010020005

typedef struct sCover {
  char          *fname;
  uint8_t       *faddr;
  uint32_t      fsize;
  struct sCover *next;
} COVER_INFO;

typedef struct {
  char *path;
  char *title;
  char *artist;
  char *album;
  int  track;
  uint32_t samples;
  lv_group_t *main_group;	// Input group when player screen is active
  lv_group_t *list_group;	// Input group when music list is active
  lv_indev_t *kdev;
  lv_obj_t *main_cont;
} MUSIC_INFO;

#define LV_DEMO_MUSIC_LANDSCAPE 1
#if LV_DEMO_MUSIC_LARGE
#  define LV_DEMO_MUSIC_HANDLE_SIZE  40
#else
#  define LV_DEMO_MUSIC_HANDLE_SIZE  20
#endif

void _lv_demo_music_play(uint32_t id);
void _lv_demo_music_pause(void);
void _lv_demo_music_resume(void);
uint32_t _lv_demo_music_get_track_count();
const char * _lv_demo_music_get_title(uint32_t track_id);
const char * _lv_demo_music_get_artist(uint32_t track_id);
const char * _lv_demo_music_get_genre(uint32_t track_id);
const char * _lv_demo_music_get_path(uint32_t track_id);
uint32_t _lv_demo_music_get_track_length(uint32_t track_id);

extern void register_cover_file(FS_DIRENT *dirent);
extern COVER_INFO *track_cover(int track);
extern uint32_t find_flash_file(char *name, int *psize);

extern MUSIC_INFO *MusicInfo;
#endif
