#ifndef __SDL_MIXER_H__
#define __SDL_MIXER_H__

#define	MIX_MAX_VOLUME	128

typedef struct Mix_Chunk {
  int     allocated;
  uint8_t *abuf;
  uint32_t alen;
  uint8_t  volume;
} Mix_Chunk;

#define	MIX_MAGIC	0x4D49584D

typedef enum {
  MST_INIT = 0,
  MST_LOADED,
  MST_PLAYING,
  MST_PAUSED,
} MUSIC_STATE;

typedef struct Mix_Music {
  const char    *fname;
  MUSIC_STATE   state;
  uint32_t magic;
} Mix_Music;

#define	MIXER_I2S_OUTPUT   (1<<0)
#define	MIXER_USB_OUTPUT   (1<<1)
#define	MIXER_SOUND_ENABLE (1<<2)
#define	MIXER_FFT_ENABLE   (1<<3)

extern void Start_SDLMixer(int mode);
extern int Mix_Started();

extern Mix_Music *Mix_LoadMUS(const char *file);
extern int Mix_PlayMusic(Mix_Music *music, int loops);
extern int Mix_ResumeMusic(Mix_Music *music);
extern int Mix_PauseMusic(Mix_Music *music);
extern int Mix_PlayingMusic();
extern void Mix_HaltMusic();
extern void Mix_FreeMusic(Mix_Music *music);
extern int Mix_VolumeMusic(int volume);

extern int Mix_PlayChannel(int channel, Mix_Chunk *chunk, int loops);
extern int Mix_PlayPosition(int channel);
extern int Mix_LoadChannel(int channel, Mix_Chunk *chunk, int loops);
extern int Mix_PauseChannel(int channel);
extern int Mix_ResumeChannel(int channel);
extern int Mix_HaltChannel(int channel);
extern int Mix_Playing(int channel);
extern int Mix_SetPanning(int channel, uint8_t left, uint8_t right);
extern void Mix_CloseAudio();
extern int Mix_AllocateChannels(int chans);
extern void Mix_FFT_Disable();

#endif
