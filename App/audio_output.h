#ifndef __AUDIO_OUTPUT_H__
#define __AUDIO_OUTPUT_H__

#include "mplayer.h"

typedef enum {
  MIX_FILL_HALF = 1,
  MIX_FILL_FULL,
  MIX_PLAY,
  MIX_PAUSE,
  MIX_RESUME,
  MIX_HALT,
  MIX_FFT_ENABLE,
  MIX_FFT_DISABLE,
  MIX_SET_VOLUME,
} mix_event;

typedef enum {
  MIX_ST_IDLE = 0,
  MIX_ST_PLAY_REQ,		// There is pending play request
  MIX_ST_PLAY,			// Music beeing played.
  MIX_ST_PAUSE,
} mix_state;

typedef struct {
  mix_state state;
  osMessageQueueId_t mixevqId;
  uint32_t  rate;
  uint32_t  ppos;		// Current play position in samples
  uint16_t  psec;		// Current play position in seconds
  uint16_t  idle_count;
} MIX_INFO;

typedef struct {
  mix_event event;
  void      *arg;
  int       option;
} MIXCONTROL_EVENT;

typedef struct s_audio_output_driver {
  void    (*Init)(struct s_audio_output_driver *pdriver);
  void    (*Start)(struct s_audio_output_driver *pdriver);
  void    (*Stop)(struct s_audio_output_driver *pdriver);
  void    (*MixSound)(struct s_audio_output_driver *pdriver, int foffset, const AUDIO_STEREO *psrc);
  void    (*SendSound)(struct s_audio_output_driver *pdriver, int foffset, const AUDIO_STEREO *psrc);
  void    (*SetVolume)(int vol);
  uint8_t *sound_buffer;
  int     sound_buffer_size;
  osMutexId_t soundLockId;
} AUDIO_OUTPUT_DRIVER;

extern MIX_INFO MixInfo;

extern AUDIO_OUTPUT_DRIVER usb_output_driver;
extern AUDIO_OUTPUT_DRIVER i2s_output_driver;
#endif
