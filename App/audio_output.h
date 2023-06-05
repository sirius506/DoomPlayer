#ifndef _AUDIO_OUTPUT_H
#define _AUDIO_OUTPUT_H

#include "mplayer.h"

typedef enum {
  MIX_DATA_REQ = 1,
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
  uint32_t  ppos;		// Current play position in samples
  uint16_t  psec;		// Current play position in seconds
  uint16_t  idle_count;
} MIX_INFO;

typedef struct {
  mix_event event;
  void      *arg;
  int       option;
} MIXCONTROL_EVENT;

struct s_audio_conf;

typedef struct {
  void    (*Init)(struct s_audio_conf *audio_conf);
  void    (*Start)(struct s_audio_conf *audio_conf);
  void    (*Stop)(struct s_audio_conf *audio_conf);
  void    (*MixSound)(struct s_audio_conf *audio_conf, const AUDIO_STEREO *psrc, int num_frame);
  void    (*SetVolume)(int vol);
} AUDIO_OUTPUT_DRIVER;

typedef struct {
  uint16_t mix_mode;
  uint16_t playRate;
  uint16_t numChan;
  uint16_t pseudoRate;
  const AUDIO_OUTPUT_DRIVER *pDriver;
} AUDIO_DEVCONF;

typedef struct s_audio_conf {
  const AUDIO_DEVCONF   *devconf;
  uint8_t         *sound_buffer;
  int             sound_buffer_size;
  uint8_t         *freebuffer_ptr;
  uint8_t         *playbuffer_ptr;
  uint8_t         play_index;		// msec position in one buffer
  osMutexId_t     soundLockId;
} AUDIO_CONF;

extern MIX_INFO MixInfo;

extern const AUDIO_OUTPUT_DRIVER usb_output_driver;
extern const AUDIO_OUTPUT_DRIVER i2s_output_driver;
#endif
