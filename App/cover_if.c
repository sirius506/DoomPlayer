#include "DoomPlayer.h"
#include "lvgl.h"
#include "app_music.h"

static COVER_INFO *cover_top;

COVER_INFO *track_cover(int track)
{
  COVER_INFO *cfp = cover_top;

  if (cfp == NULL)
    return NULL;

  if (track < 0) track = 0;
  while (track > 0)
  {
    track--;
    cfp = cfp->next;
    if (cfp == NULL)
      cfp = cover_top;
  }
  return cfp;
}

void register_cover_file(FS_DIRENT *dirent)
{
  COVER_INFO *cfp;
  int i;
  uint32_t *bp;
  char mbuffer[20];

  cover_top = NULL;

  for (i = 1; i < NUM_DIRENT; i++)
  {
    if (dirent->foffset == 0xFFFFFFFF)
      break; 
    memcpy(mbuffer, dirent->fname, FS_NAMELEN);
    if (strncasecmp(mbuffer + strlen(mbuffer) - 4, ".bin", 4) == 0)
    {
      bp = (uint32_t *)(QSPI_ADDR + dirent->foffset);
      if (*bp == COVER_MAGIC)
      {
        cfp = (COVER_INFO *)lv_mem_alloc(sizeof(COVER_INFO));
        cfp->fname = dirent->fname;
        cfp->faddr = (uint8_t *) bp;
        cfp->fsize = dirent->fsize;
        cfp->next = cover_top;
        cover_top = cfp;
      }
    }
    dirent++;
  }
}
