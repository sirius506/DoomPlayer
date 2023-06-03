#ifndef _MPLAYER_H
#define _MPLAYER_H

#include "fatfs.h"
#include "../SDL/SDL.h"
#include "../SDL/SDL_mixer.h"

#define	EVF_START	(1<<8)
#define	EVF_STOP	(1<<9)
#define	EVF_SOF		(1<<10)
#define	EVF_VOLUME	(1<<11)

#define	BUF_MSEC	10	/* Hold 10ms worth PCM samples in one frame */
#define	NUM_FRAMES	480	/* 10ms worth PCM samples (48 * 10 = 480) */
#define	NUM_32KFRAMES	320	/* 10ms worth PCM samples at 32KHz */
#define	BUF_FACTOR	2	/* Two for double buffering */
#define	BUF_FRAMES	(NUM_FRAMES*BUF_FACTOR)

#define	FFT_DECIMATION_FACTOR	3

#define	OUT_SAMPLES	(NUM_FRAMES/FFT_DECIMATION_FACTOR)
#define	AUDIO_SAMPLES	3200	

#define	FFT_SAMPLES	2048
#define	SHIFT_SAMPLES	256
#define	MAGDIV		10

typedef struct {
  int16_t ch0;
  int16_t ch1;
} AUDIO_STEREO;

typedef struct {
  int16_t ch0;
  int16_t ch1;
  int16_t ch2;
  int16_t ch3;
} AUDIO_4CH;

#define	NUM_CHANNELS	16

typedef struct {
  int flag;
  Mix_Chunk *chunk;
  int loop;
  uint8_t   vol_left, vol_right;
  AUDIO_STEREO   *pread;
  AUDIO_STEREO   *plast;
} CHANINFO;

#define	FL_ALLOCED	(1<<0)
#define	FL_SET		(1<<1)
#define	FL_PLAY		(1<<2)
#define	FL_32K		(1<<3)

extern CHANINFO ChanInfo[NUM_CHANNELS];
extern void mix_request_data(int full);
extern void src32k_init();
extern int src32k_process(AUDIO_STEREO *psrc, int in_frames);

#endif
