/**
 * @file audio_output.c
 *
 * @brief DoomPlayer Audio output driver
 */
#include "DoomPlayer.h"
#include "board_if.h"
#include "audio_output.h"
#include "usbh_audio.h"

static SECTION_AUDIOBUFF AUDIO_4CH FinalSoundBuffer[BUF_FRAMES];

static void usb_output_init(AUDIO_OUTPUT_DRIVER *pDriver)
{
  pDriver->sound_buffer = (uint8_t *)FinalSoundBuffer;
  pDriver->sound_buffer_size = BUF_FRAMES * sizeof(AUDIO_4CH);
  memset(pDriver->sound_buffer, 0, pDriver->sound_buffer_size);
}

static void i2s_output_init(AUDIO_OUTPUT_DRIVER *pDriver)
{
  Board_Audio_Init();

  pDriver->sound_buffer = (uint8_t *)FinalSoundBuffer;
  pDriver->sound_buffer_size = BUF_FRAMES * sizeof(AUDIO_STEREO);
  memset(pDriver->sound_buffer, 0, pDriver->sound_buffer_size);
}

static void usb_output_start(AUDIO_OUTPUT_DRIVER *pDriver)
{
  UsbAudio_Output_Start(pDriver->sound_buffer, pDriver->sound_buffer_size);
}

static void i2s_output_start(AUDIO_OUTPUT_DRIVER *pDriver)
{
  Board_Audio_Output_Start((uint16_t *)pDriver->sound_buffer, pDriver->sound_buffer_size);
}

static void usb_output_stop(AUDIO_OUTPUT_DRIVER *pDriver)
{
}

static void i2s_output_stop(AUDIO_OUTPUT_DRIVER *pDriver)
{
  Board_Audio_Output_Stop();
}

static void i2s_mix_sound(AUDIO_OUTPUT_DRIVER *pDriver, int frame_offset, const AUDIO_STEREO *psrc)
{
  int i, c;
  CHANINFO *chanInfo = ChanInfo;
  int sample_left, sample_right;
  AUDIO_STEREO *pdst, *pwork;

  pwork = pdst = (AUDIO_STEREO *)(pDriver->sound_buffer + frame_offset * sizeof(AUDIO_STEREO));
  osMutexAcquire(pDriver->soundLockId, osWaitForever);

  for (i = 0; i < BUF_FRAMES/2; i++)
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
  osMutexRelease(pDriver->soundLockId);
  SCB_CleanDCache_by_Addr((uint32_t *)pwork, 4 * BUF_FRAMES/2);
}

static void i2s_send_sound(AUDIO_OUTPUT_DRIVER *pDriver, int frame_offset, const AUDIO_STEREO *psrc)
{
  int i;
  AUDIO_STEREO *pdst, *pwork;

  pwork = pdst = (AUDIO_STEREO *)(pDriver->sound_buffer + frame_offset * sizeof(AUDIO_STEREO));
  osMutexAcquire(pDriver->soundLockId, osWaitForever);

  for (i = 0; i < BUF_FRAMES/2; i++)
  {
    pdst->ch0 = psrc->ch0;
    pdst->ch1 = psrc->ch1;
    psrc++;
    pdst++;
  }
  osMutexRelease(pDriver->soundLockId);
  SCB_CleanDCache_by_Addr((uint32_t *)pwork, 4 * BUF_FRAMES/2);
}

static void i2s_set_volume(int vol)
{
  Board_SetVolume(vol);
}

static void usb_mix_sound(AUDIO_OUTPUT_DRIVER *pDriver, int frame_offset, const AUDIO_STEREO *psrc)
{
  int i, c;
  CHANINFO *chanInfo = ChanInfo;
  int sample_left, sample_right;
  AUDIO_4CH *pdst, *pwork;

  pwork = pdst = (AUDIO_4CH *)(pDriver->sound_buffer + frame_offset * sizeof(AUDIO_4CH));
  osMutexAcquire(pDriver->soundLockId, osWaitForever);

  for (i = 0; i < BUF_FRAMES/2; i++)
  {
    chanInfo = ChanInfo;

    sample_left = sample_right = 0;

#define DO_MIX_SOUND
#ifdef DO_MIX_SOUND
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
#endif
    pdst->ch0 = (psrc->ch0 + sample_left);
    pdst->ch1 = (psrc->ch1 + sample_right);
    pdst->ch2 = sample_left;
    pdst->ch3 = sample_right;
    psrc++;
    pdst++;
  }
  SCB_CleanInvalidateDCache_by_Addr((uint32_t *)pwork, 4 * BUF_FRAMES/2);
  osMutexRelease(pDriver->soundLockId);
}

static void usb_send_sound(AUDIO_OUTPUT_DRIVER *pDriver, int frame_offset, const AUDIO_STEREO *psrc)
{
  int i;
  AUDIO_4CH *pdst, *pwork;

  pwork = pdst = (AUDIO_4CH *)(pDriver->sound_buffer + frame_offset * sizeof(AUDIO_4CH));
  osMutexAcquire(pDriver->soundLockId, osWaitForever);

  for (i = 0; i < BUF_FRAMES/2; i++)
  {
    pdst->ch0 = psrc->ch0;
    pdst->ch1 = psrc->ch1;
    pdst->ch2 = 0;
    pdst->ch3 = 0;
    psrc++;
    pdst++;
  }
  SCB_CleanInvalidateDCache_by_Addr((uint32_t *)pwork, 4 * BUF_FRAMES/2);
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
  .SendSound = usb_send_sound,
  .SetVolume = usb_set_volume,
};

AUDIO_OUTPUT_DRIVER i2s_output_driver = {
  .Init = i2s_output_init,
  .Start = i2s_output_start,
  .Stop = i2s_output_stop,
  .MixSound = i2s_mix_sound,
  .SendSound = i2s_send_sound,
  .SetVolume = i2s_set_volume,
};

