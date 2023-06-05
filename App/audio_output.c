/**
 * @file audio_output.c
 *
 * @brief DoomPlayer Audio output driver
 */
#include "DoomPlayer.h"
#include "board_if.h"
#include "audio_output.h"
#include "usbh_audio.h"

extern void UsbAudio_Output_Start(AUDIO_CONF *audio_conf);

/*
 * Mixered sounds data is stored into FinalSoundBuffer,
 * that has 4ch space for DualSense.
 * I2S and DualShock driver will use only half (2ch) space.
 */
static SECTION_AUDIOBUFF AUDIO_4CH FinalSoundBuffer[BUF_FRAMES];

/**
 * @brief Initialize USB audio output driver
 */
static void usb_output_init(AUDIO_CONF *audio_conf)
{
  audio_conf->sound_buffer = (uint8_t *)FinalSoundBuffer;
  if (audio_conf->devconf->numChan == 4)
    audio_conf->sound_buffer_size = BUF_FRAMES * sizeof(AUDIO_4CH);
  else
  {
    if (audio_conf->devconf->playRate == 32000)
      audio_conf->sound_buffer_size = NUM_32KFRAMES * 2 * sizeof(AUDIO_STEREO);
    else
      audio_conf->sound_buffer_size = BUF_FRAMES * sizeof(AUDIO_STEREO);
  }

  audio_conf->freebuffer_ptr = audio_conf->sound_buffer;
  audio_conf->playbuffer_ptr = audio_conf->sound_buffer;
  audio_conf->play_index = BUF_MSEC;
  memset(audio_conf->sound_buffer, 0, audio_conf->sound_buffer_size);
}

/**
 * @brief Initialize I2S audio output driver
 */
static void i2s_output_init(AUDIO_CONF *audio_conf)
{
  Board_Audio_Init();

  audio_conf->sound_buffer = (uint8_t *)FinalSoundBuffer;
  audio_conf->sound_buffer_size = BUF_FRAMES * sizeof(AUDIO_STEREO);

  audio_conf->freebuffer_ptr = audio_conf->sound_buffer;
  audio_conf->playbuffer_ptr = audio_conf->sound_buffer;
  audio_conf->play_index = BUF_MSEC;

  memset(audio_conf->sound_buffer, 0, audio_conf->sound_buffer_size);
}

static void usb_output_start(AUDIO_CONF *audio_conf)
{
  UsbAudio_Output_Start(audio_conf);
}

static void i2s_output_start(AUDIO_CONF *audio_conf)
{
  Board_Audio_Output_Start((uint16_t *)audio_conf->sound_buffer, audio_conf->sound_buffer_size);
}

static void usb_output_stop(AUDIO_CONF *audio_conf)
{
}

static void i2s_output_stop(AUDIO_CONF *audio_conf)
{
  Board_Audio_Output_Stop();
}

static void i2s_mix_sound(AUDIO_CONF *audio_conf, const AUDIO_STEREO *psrc, int num_frame)
{
  int i, c;
  CHANINFO *chanInfo = ChanInfo;
  int sample_left, sample_right;
  AUDIO_STEREO *pdst;
  osMutexAcquire(audio_conf->soundLockId, osWaitForever);
  pdst = (AUDIO_STEREO *)(audio_conf->freebuffer_ptr);

  for (i = 0; i < num_frame; i++)
  {
      chanInfo = ChanInfo;

      sample_left = sample_right = 0;

      for (c = 0; c < NUM_CHANNELS; c++)
      {
        if (chanInfo->flag & FL_SET)
        {
          if (chanInfo->pread < chanInfo->plast)
          {
            sample_left += (chanInfo->pread->ch0 * chanInfo->vol_left) / MIX_MAX_VOLUME;
            sample_right += (chanInfo->pread->ch1 * chanInfo->vol_right) / MIX_MAX_VOLUME;
            chanInfo->pread++;
            if (chanInfo->pread >= chanInfo->plast)
            {
              chanInfo->pread = (AUDIO_STEREO *)chanInfo->chunk->abuf;
              chanInfo->flag &= ~(FL_PLAY|FL_SET);
#ifdef MIX_DEBUG
              debug_printf("chan %d done.\n", c);
#endif
              c = NUM_CHANNELS;
            }
          }
        }
        chanInfo++;
      }
#if 0
      pdst->ch0 = (psrc->ch0 + sample_left)/2;
      pdst->ch1 = (psrc->ch1 + sample_right)/2;
#else
      pdst->ch0 = (psrc->ch0 + sample_left);
      pdst->ch1 = (psrc->ch1 + sample_right);
#endif
      psrc++;
      pdst++;
  }

  SCB_CleanInvalidateDCache_by_Addr((uint32_t *)audio_conf->freebuffer_ptr, sizeof(AUDIO_STEREO)*NUM_FRAMES);

  audio_conf->freebuffer_ptr += NUM_FRAMES * sizeof(AUDIO_STEREO);
  if (audio_conf->freebuffer_ptr >= audio_conf->sound_buffer + audio_conf->sound_buffer_size)
      audio_conf->freebuffer_ptr = audio_conf->sound_buffer;

  osMutexRelease(audio_conf->soundLockId);
}


static void i2s_set_volume(int vol)
{
  Board_SetVolume(vol);
}

static void usb_mix_sound4ch(AUDIO_CONF *audio_conf, const AUDIO_STEREO *psrc, int num_frame)
{
  int i, c;
  CHANINFO *chanInfo = ChanInfo;
  int sample_left, sample_right;
  AUDIO_4CH *pdst;
  osMutexAcquire(audio_conf->soundLockId, osWaitForever);
  pdst = (AUDIO_4CH *)(audio_conf->freebuffer_ptr);

  for (i = 0; i < num_frame; i++)
  {
      chanInfo = ChanInfo;

      sample_left = sample_right = 0;

      for (c = 0; c < NUM_CHANNELS; c++)
      {
        if (chanInfo->flag & FL_SET)
        {
          if (chanInfo->pread < chanInfo->plast)
          {
            sample_left += (chanInfo->pread->ch0 * chanInfo->vol_left) / MIX_MAX_VOLUME;
            sample_right += (chanInfo->pread->ch1 * chanInfo->vol_right) / MIX_MAX_VOLUME;
            chanInfo->pread++;
            if (chanInfo->pread >= chanInfo->plast)
            {
              chanInfo->pread = (AUDIO_STEREO *)chanInfo->chunk->abuf;
              chanInfo->flag &= ~(FL_PLAY|FL_SET);
#ifdef MIX_DEBUG
              debug_printf("chan %d done.\n", c);
#endif
              c = NUM_CHANNELS;
            }
          }
        }
        chanInfo++;
      }
      pdst->ch0 = (psrc->ch0 + sample_left);
      pdst->ch1 = (psrc->ch1 + sample_right);
      pdst->ch2 = sample_left;
      pdst->ch3 = sample_right;
      psrc++;
      pdst++;
  }

  audio_conf->freebuffer_ptr += NUM_FRAMES * sizeof(AUDIO_4CH);
  if (audio_conf->freebuffer_ptr >= audio_conf->sound_buffer + audio_conf->sound_buffer_size)
      audio_conf->freebuffer_ptr = audio_conf->sound_buffer;
  osMutexRelease(audio_conf->soundLockId);
}

static void usb_mix_sound(AUDIO_CONF *audio_conf, const AUDIO_STEREO *psrc, int num_frame)
{
  int i, c;
  CHANINFO *chanInfo = ChanInfo;
  int sample_left, sample_right;
  AUDIO_STEREO *pdst;

  if (audio_conf->devconf->numChan == 4)
  {
    /* DualSense has four channels output. */
    usb_mix_sound4ch(audio_conf, psrc, num_frame);
    return;
  }

  osMutexAcquire(audio_conf->soundLockId, osWaitForever);

  pdst = (AUDIO_STEREO *)(audio_conf->freebuffer_ptr);

  for (i = 0; i < num_frame; i++)
  {
      chanInfo = ChanInfo;

      sample_left = sample_right = 0;

      for (c = 0; c < NUM_CHANNELS; c++)
      {
        if (chanInfo->flag & FL_SET)
        {
          if (chanInfo->pread < chanInfo->plast)
          {
            sample_left += (chanInfo->pread->ch0 * chanInfo->vol_left) / MIX_MAX_VOLUME;
            sample_right += (chanInfo->pread->ch1 * chanInfo->vol_right) / MIX_MAX_VOLUME;
            chanInfo->pread++;
            if (chanInfo->pread >= chanInfo->plast)
            {
              chanInfo->pread = (AUDIO_STEREO *)chanInfo->chunk->abuf;
              chanInfo->flag &= ~(FL_PLAY|FL_SET);
#ifdef MIX_DEBUG
              debug_printf("chan %d done.\n", c);
#endif
              c = NUM_CHANNELS;
            }
          }
        }
        chanInfo++;
      }
      pdst->ch0 = (psrc->ch0 + sample_left);
      pdst->ch1 = (psrc->ch1 + sample_right);
      psrc++;
      pdst++;
  }

  audio_conf->freebuffer_ptr += num_frame * sizeof(AUDIO_STEREO);
  if (audio_conf->freebuffer_ptr >= audio_conf->sound_buffer + audio_conf->sound_buffer_size)
      audio_conf->freebuffer_ptr = audio_conf->sound_buffer;
  osMutexRelease(audio_conf->soundLockId);
}

static void usb_set_volume(int vol)
{
  usbAudio_SetVolume(vol);
}

const AUDIO_OUTPUT_DRIVER usb_output_driver = {
  .Init = usb_output_init,
  .Start = usb_output_start,
  .Stop = usb_output_stop,
  .MixSound = usb_mix_sound,
  .SetVolume = usb_set_volume,
};

const AUDIO_OUTPUT_DRIVER i2s_output_driver = {
  .Init = i2s_output_init,
  .Start = i2s_output_start,
  .Stop = i2s_output_stop,
  .MixSound = i2s_mix_sound,
  .SetVolume = i2s_set_volume,
};

