/**
 * @file audio_output.c
 *
 * @brief DoomPlayer Audio output driver
 */
#include "DoomPlayer.h"
#include "board_if.h"
#include "audio_output.h"
#include "usbh_audio.h"

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
  AUDIO_OUTPUT_DRIVER *pDriver = audio_conf->pDriver;

  pDriver->sound_buffer = (uint8_t *)FinalSoundBuffer;
  if (audio_conf->numChan == 4)
    pDriver->sound_buffer_size = BUF_FRAMES * sizeof(AUDIO_4CH);
  else
  {
    if (audio_conf->playRate == 32000)
      pDriver->sound_buffer_size = NUM_32KFRAMES * 2 * sizeof(AUDIO_STEREO);
    else
      pDriver->sound_buffer_size = BUF_FRAMES * sizeof(AUDIO_STEREO);
  }

  pDriver->freebuffer_ptr = pDriver->sound_buffer;
  pDriver->playbuffer_ptr = pDriver->sound_buffer;
  pDriver->play_index = BUF_MSEC;
  memset(pDriver->sound_buffer, 0, pDriver->sound_buffer_size);
}

/**
 * @brief Initialize I2S audio output driver
 */
static void i2s_output_init(AUDIO_CONF *audio_conf)
{
  AUDIO_OUTPUT_DRIVER *pDriver = audio_conf->pDriver;

  Board_Audio_Init();

  pDriver->sound_buffer = (uint8_t *)FinalSoundBuffer;
  pDriver->sound_buffer_size = BUF_FRAMES * sizeof(AUDIO_STEREO);

  pDriver->freebuffer_ptr = pDriver->sound_buffer;
  pDriver->playbuffer_ptr = pDriver->sound_buffer;
  pDriver->play_index = BUF_MSEC;

  memset(pDriver->sound_buffer, 0, pDriver->sound_buffer_size);
}

static void usb_output_start(AUDIO_CONF *audio_conf)
{
  UsbAudio_Output_Start(audio_conf);
}

static void i2s_output_start(AUDIO_CONF *audio_conf)
{
  AUDIO_OUTPUT_DRIVER *pDriver = audio_conf->pDriver;

  Board_Audio_Output_Start((uint16_t *)pDriver->sound_buffer, pDriver->sound_buffer_size);
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
  AUDIO_OUTPUT_DRIVER *pDriver = audio_conf->pDriver;

  osMutexAcquire(pDriver->soundLockId, osWaitForever);
  pdst = (AUDIO_STEREO *)(pDriver->freebuffer_ptr);

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

  SCB_CleanInvalidateDCache_by_Addr((uint32_t *)pDriver->freebuffer_ptr, sizeof(AUDIO_STEREO)*NUM_FRAMES);

  pDriver->freebuffer_ptr += NUM_FRAMES * sizeof(AUDIO_STEREO);
  if (pDriver->freebuffer_ptr >= pDriver->sound_buffer + pDriver->sound_buffer_size)
      pDriver->freebuffer_ptr = pDriver->sound_buffer;

  osMutexRelease(pDriver->soundLockId);
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
  AUDIO_OUTPUT_DRIVER *pDriver = audio_conf->pDriver;

  osMutexAcquire(pDriver->soundLockId, osWaitForever);
  pdst = (AUDIO_4CH *)(pDriver->freebuffer_ptr);

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

  pDriver->freebuffer_ptr += NUM_FRAMES * sizeof(AUDIO_4CH);
  if (pDriver->freebuffer_ptr >= pDriver->sound_buffer + pDriver->sound_buffer_size)
      pDriver->freebuffer_ptr = pDriver->sound_buffer;
  osMutexRelease(pDriver->soundLockId);
}

static void usb_mix_sound(AUDIO_CONF *audio_conf, const AUDIO_STEREO *psrc, int num_frame)
{
  int i, c;
  CHANINFO *chanInfo = ChanInfo;
  int sample_left, sample_right;
  AUDIO_STEREO *pdst;
  AUDIO_OUTPUT_DRIVER *pDriver = audio_conf->pDriver;

  if (audio_conf->numChan == 4)
  {
    /* DualSense has four channels output. */
    usb_mix_sound4ch(audio_conf, psrc, num_frame);
    return;
  }

  osMutexAcquire(pDriver->soundLockId, osWaitForever);

  pdst = (AUDIO_STEREO *)(pDriver->freebuffer_ptr);

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

  pDriver->freebuffer_ptr += num_frame * sizeof(AUDIO_STEREO);
  if (pDriver->freebuffer_ptr >= pDriver->sound_buffer + pDriver->sound_buffer_size)
      pDriver->freebuffer_ptr = pDriver->sound_buffer;
  osMutexRelease(pDriver->soundLockId);
}

static void usb_set_volume(int vol)
{
  usbAudio_SetVolume(vol);
}

AUDIO_OUTPUT_DRIVER usb_output_driver = {
  .Init = usb_output_init,
  .Start = usb_output_start,
  .Stop = usb_output_stop,
  .MixSound = usb_mix_sound,
  .SetVolume = usb_set_volume,
};

AUDIO_OUTPUT_DRIVER i2s_output_driver = {
  .Init = i2s_output_init,
  .Start = i2s_output_start,
  .Stop = i2s_output_stop,
  .MixSound = i2s_mix_sound,
  .SetVolume = i2s_set_volume,
};

