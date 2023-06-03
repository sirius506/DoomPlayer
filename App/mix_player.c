/**
 *  SDL based Music player task and SDL mixer API support routines.
 */
#include "DoomPlayer.h"
#include "mplayer.h"
#include "app_task.h"
#include <arm_math.h>
#include <stdlib.h>

#define DR_FLAC_IMPLEMENTATION
#define	DR_FLAC_NO_CRC
#define	DR_FLAC_NO_STDIO
#define	DR_FLAC_NO_SIMD
#define	DR_FLAC_NO_OGG

#define	IDLE_PERIOD	50

#define	DO_FFT

#include "dr_flac.h"
#include "audio_output.h"
#include "src32k.h"

#define PLAYER_STACK_SIZE  1400

TASK_DEF(mixplayer, PLAYER_STACK_SIZE, osPriorityAboveNormal)

/*
 * Buffers to store music source data.
 * FLAC music data is read into MusicFrameBuffer.
 * SilentBuffer is used to generate silience.
 */
static SECTION_AUDIOSRAM AUDIO_STEREO MusicFrameBuffer[BUF_FRAMES];
static const AUDIO_STEREO SilentBuffer[BUF_FRAMES/2];

#define	NUMTAPS	31

SECTION_DTCMRAM int16_t   sInBuffer[BUF_FRAMES];
SECTION_DTCMRAM float32_t sFloatBuffer[BUF_FRAMES];
SECTION_DTCMRAM float32_t DeciStateBuffer[BUF_FRAMES+NUMTAPS-1];

SECTION_PREFFT float32_t audio_buffer[AUDIO_SAMPLES];	// Decimated Audio samples
/*
 * Buffers for FFT process
 * Note that result buffer contains complex numbers, but its size is same as
 * input butter, because arm_rfft_fast_f32() process takes advantage of the
 * symmetry properties of the FFT.
 */
SECTION_DTCMRAM float32_t fft_real_buffer[FFT_SAMPLES];		// FFT input buffer (only real part)
SECTION_DTCMRAM float32_t fft_result_buffer[FFT_SAMPLES];	// FFT resut buffer (complex number)

SECTION_DTCMRAM float32_t float_mag_buffer[FFT_SAMPLES/2];

const float32_t Coeffs[NUMTAPS] = {
  0.00156601, -0.00182149,  0.00249659, -0.00359653,  0.00510147, -0.0069667,
  0.00912439, -0.01148685,  0.01395103, -0.01640417,  0.01873013, -0.02081595,
  0.02255847, -0.02387033,  0.02468516,  0.97349755,  0.02468516, -0.02387033,
  0.02255847, -0.02081595,  0.01873013, -0.01640417,  0.01395103, -0.01148685,
  0.00912439, -0.0069667,   0.00510147, -0.00359653,  0.00249659, -0.00182149,
  0.00156601
};

typedef struct {
  float32_t *putptr;
  float32_t *getptr;
  int16_t samples;      /* Accumurated data samples */
  osMessageQueueId_t *fftqId;
} FFTINFO;

SECTION_DTCMRAM FFTINFO FftInfo;

typedef struct {
  FIL  *pfile;
  char *comments;
  int  loop_count;
  int  loop_start;
  int  loop_end;
  int  pcm_pos;
} FLACINFO;

SECTION_DTCMRAM FLACINFO FlacInfo;

static SECTION_DTCMRAM arm_fir_decimate_instance_f32 decimate_instance;
static SECTION_DTCMRAM arm_rfft_fast_instance_f32 fft_instance;

SECTION_DTCMRAM MIX_INFO MixInfo;

#define	MIX_EV_DEPTH	5

MUTEX_DEF(sound_lock)
static osMutexId_t soundLockId;

static MIXCONTROL_EVENT mix_buffer[MIX_EV_DEPTH];

MESSAGEQ_DEF(mixevq, mix_buffer, sizeof(mix_buffer))

#define	ALLOC_BUFFER_SIZE	(1024*40)

SECTION_FLACPOOL uint8_t FlacAllocSpace[ALLOC_BUFFER_SIZE];

static uint8_t *flac_allocp;

static void *my_malloc(size_t sz, void *pUserData)
{
  void *p;

  sz = (sz + 3) & ~3;		// Align on word boundary
// debug_printf("%s: %d, %x\n", __FUNCTION__, sz, flac_allocp);
  if (flac_allocp + sz > &FlacAllocSpace[ALLOC_BUFFER_SIZE])
  {
    p = NULL;
    debug_printf("alloc failed.\n");
    osDelay(10*1000);
  }
  else
  {
    p = flac_allocp;
    flac_allocp += sz;
  }
  return p;
}

static void *my_realloc(void *p, size_t sz, void *pUserData)
{
  void *vp;

  if (p == NULL)
  {
    vp = flac_allocp;
    debug_printf("%s: %d --> %x\n", __FUNCTION__, sz, vp);
    flac_allocp += sz;
    return vp;
  }
// debug_printf("%s: %d\n", __FUNCTION__, sz);

  return NULL;
}

static void my_free(void *p, void *pUserData)
{
  // debug_printf("my_free %x\n", p);
}

static size_t drflac__on_read_fatfs(void* pUserData, void* pBufferOut, size_t bytesToRead)
{
    UINT nread;
    FRESULT res;
    FLACINFO *piflac = (FLACINFO *)pUserData;

    res = f_read(piflac->pfile, pBufferOut, bytesToRead, &nread);
    if (res == FR_OK)
       return nread;
    return -1;
}

static drflac_bool32 drflac__on_seek_fatfs(void* pUserData, int offset, drflac_seek_origin origin)
{
    FRESULT res;
    FLACINFO *piflac = (FLACINFO *)pUserData;

    if (origin == drflac_seek_origin_current)
      res = f_lseek(piflac->pfile, f_tell(piflac->pfile) + offset);
    else
      res = f_lseek(piflac->pfile, offset);

    return (res == FR_OK);
}

/*===========================*/

static drflac_result drflac_fatopen(FIL** ppFile, const char* pFilePath)
{
    if (ppFile != NULL) {
        *ppFile = NULL;  /* Safety. */
    }

    if (pFilePath == NULL || ppFile == NULL) {
        return DRFLAC_INVALID_ARGS;
    }

    *ppFile = OpenFlacFile((char *)pFilePath);
    if (*ppFile == NULL) {
        drflac_result result = DRFLAC_DOES_NOT_EXIST;
        if (result == DRFLAC_SUCCESS) {
            result = DRFLAC_ERROR;   /* Just a safety check to make sure we never ever return success when pFile == NULL. */
        }

        return result;
    }

    return DRFLAC_SUCCESS;
}

static drflac_vorbis_comment_iterator CommentIterator;

static void drflac__on_meta(void* pUserData, drflac_metadata* pMetadata)
{
    FLACINFO *piflac = (FLACINFO *)pUserData;

    if (pMetadata->type == DRFLAC_METADATA_BLOCK_TYPE_VORBIS_COMMENT)
    {
      const char *cp;
      char *wp, *dp;
      drflac_uint32 clen;

      drflac_init_vorbis_comment_iterator(&CommentIterator, pMetadata->data.vorbis_comment.commentCount, pMetadata->data.vorbis_comment.pComments);
      dp = piflac->comments = (char *)my_malloc(pMetadata->rawDataSize, NULL);
      while (CommentIterator.countRemaining > 0)
      {
        cp = drflac_next_vorbis_comment(&CommentIterator, &clen);
        if (clen > 0)
        {
          memcpy(dp, cp, clen);
          wp = dp;
          dp += clen;
          *dp++ = 0;
          // debug_printf("%s\n", wp);
          if (strncmp(wp, "LOOP_START=", 11) == 0)
            piflac->loop_start = atoi(wp+11);
          else if (strncmp(wp, "LOOP_END=", 9) == 0)
            piflac->loop_end = atoi(wp+9);
        }
      }
    }
}

static drflac* drflac_open_fatfile(const char* pFileName, const drflac_allocation_callbacks* pAllocationCallbacks)
{
    drflac* pFlac;
    FLACINFO *piflac;
    FIL* pFile;

    flac_allocp = &FlacAllocSpace[0];

    if (drflac_fatopen(&pFile, pFileName) != DRFLAC_SUCCESS) {
        return NULL;
    }
    piflac = (FLACINFO *)pAllocationCallbacks->pUserData;
    piflac->pfile = pFile;
    piflac->loop_start = piflac->loop_end = piflac->pcm_pos = 0;

    pFlac = drflac_open_with_metadata(drflac__on_read_fatfs, drflac__on_seek_fatfs,  drflac__on_meta, (void*)piflac, pAllocationCallbacks);
    if (pFlac == NULL) {
        CloseFlacFile(pFile);
        return NULL;
    }

    return pFlac;
}

static void drflac_close_fatfs(drflac* pFlac)
{
    if (pFlac == NULL) {
        return;
    }

    FLACINFO *piflac;
    piflac = (FLACINFO *)pFlac->bs.pUserData;

    /*
    If we opened the file with drflac_open_file() we will want to close the file handle. We can know whether or not drflac_open_file()
    was used by looking at the callbacks.
    */
    if (pFlac->bs.onRead == drflac__on_read_fatfs) {
        CloseFlacFile(piflac->pfile);
    }

#ifndef DR_FLAC_NO_OGG
    /* Need to clean up Ogg streams a bit differently due to the way the bit streaming is chained. */
    if (pFlac->container == drflac_container_ogg) {
        drflac_oggbs* oggbs = (drflac_oggbs*)pFlac->_oggbs;
        DRFLAC_ASSERT(pFlac->bs.onRead == drflac__on_read_ogg);

        if (oggbs->onRead == drflac__on_read_fatfs) {
            CloseFlacFile((FIL*)oggbs->pUserData);
        }
    }
#endif

    drflac__free_from_callbacks(pFlac, &pFlac->allocationCallbacks);
}

static const int frange[] = { 8, 45, 300, 600, 0 };
static int band_val[4];

int fft_getband(int band)
{
  return band_val[band];
}

int fft_getcolor(uint8_t *p)
{
  int v;

#if 0
  v = band_val[0];
  *p++ = (v > 255)? 255 : v;
  v = band_val[1];
  *p++ = (v > 255)? 255 : v;
  v = band_val[2] + band_val[3];
  *p = (v > 255)? 255 : v;
#else
  v = band_val[0] + band_val[1];
  v <<= 1;
  *p++ = (v > 255)? 255 : v;
  v = band_val[2];
  v <<= 1;
  *p++ = (v > 255)? 255 : v;
  v = band_val[3];
  v <<= 1;
  *p = (v > 255)? 255 : v;
#endif
  v = band_val[0] + band_val[1] + band_val[2] + band_val[3];
  return v;
}

/*
 *  FFT process.
 *   Original sampling rate is 44.1k/48k. We reduce the rate to 1/3,
 *   resulting as 14.7k/16k
 */
static int process_fft(FFTINFO *fftInfo, AUDIO_STEREO *pmusic, int frames)
{
  int i;

  /* Convert to mono */
  for (i = 0; i < frames; i++)
  {
    sInBuffer[i] = (pmusic->ch0 + pmusic->ch1) / 2;
    pmusic++;
  }

  /* Convert int16_t values to float, then decimate */
  arm_q15_to_float(sInBuffer, sFloatBuffer, frames);
  arm_fir_decimate_f32(&decimate_instance, sFloatBuffer, fftInfo->putptr, frames);

  fftInfo->putptr += (frames/FFT_DECIMATION_FACTOR);
  if (fftInfo->putptr >= &audio_buffer[AUDIO_SAMPLES])
  {
    fftInfo->putptr = audio_buffer;
  }
  fftInfo->samples += OUT_SAMPLES;

  if (fftInfo->samples >= FFT_SAMPLES)
  {
    int room;

//    debug_printf("putptr: %x, getptr: %x, samples = %d\n", fftInfo->putptr, fftInfo->getptr, fftInfo->samples);
    room = &audio_buffer[AUDIO_SAMPLES] - fftInfo->getptr;
    if (room >= FFT_SAMPLES)
    {
          memcpy(fft_real_buffer, fftInfo->getptr, sizeof(float32_t) * FFT_SAMPLES);
    }
    else
    {
          memcpy(fft_real_buffer, fftInfo->getptr, sizeof(float32_t) * room);
          memcpy(fft_real_buffer + room, audio_buffer, sizeof(float32_t) * (FFT_SAMPLES - room));
    }
    arm_rfft_fast_f32(&fft_instance, fft_real_buffer, fft_result_buffer, 0);
    arm_cmplx_mag_f32(fft_result_buffer, float_mag_buffer, FFT_SAMPLES/2);

    {
      int f, f_prev;
      const int *fp;
      int *op;
      float32_t v;

      f_prev = 0;
      fp = frange;
      op = band_val;
      while (*fp)
      {
        f = *fp++;
        v = 0.0;
        for (i = f_prev; i < f; i++)
        {
          v += float_mag_buffer[i];
        }
        if (v) v = v / MAGDIV;
        if (v < 0) v = 0;
        f_prev = f;
        *op++ = (int16_t)v;
      }
    //debug_printf("BAND: %d, %d, %d, %d\n", band_val[0], band_val[1], band_val[2], band_val[3]);
    }

    fftInfo->getptr += SHIFT_SAMPLES;
    if (fftInfo->getptr >= &audio_buffer[AUDIO_SAMPLES])
      fftInfo->getptr -= AUDIO_SAMPLES;
    fftInfo->samples -= SHIFT_SAMPLES; 
    return 1;	/* New FFT result is ready. */
  }
  return 0;	/* No FFT result available. */
}

static void StartMixPlayerTask(void *args)
{
  drflac_uint64 num_read;
  drflac_allocation_callbacks allocationCallbacks;
  drflac *pflac = NULL;
  AUDIO_STEREO *pmusic;
  AUDIO_OUTPUT_DRIVER *pDriver;
  FFTINFO *fftInfo;
  MIX_INFO *mixInfo = &MixInfo;
  FLACINFO *flacInfo = &FlacInfo;
  int fft_count;
  GUI_EVENT guiev;
  int argval;
  uint32_t psec;
  AUDIO_CONF *audio_config;
  int mix_frames;

  debug_printf("Player Started..\n");

  audio_config = (AUDIO_CONF *)args;
  argval = audio_config->mix_mode;

  /* Prepare silent sound data. */
  memset((void *)SilentBuffer, 0, sizeof(SilentBuffer));


  fftInfo = &FftInfo;
  fft_count = 0;
  arm_fir_decimate_init_f32(&decimate_instance, NUMTAPS, FFT_DECIMATION_FACTOR, (float32_t *)Coeffs, DeciStateBuffer, BUF_FRAMES/2);

  allocationCallbacks.pUserData = flacInfo;
  allocationCallbacks.onMalloc = my_malloc;
  allocationCallbacks.onRealloc = my_realloc;
  allocationCallbacks.onFree = my_free;

  pDriver = (AUDIO_OUTPUT_DRIVER *)audio_config->pDriver;
  if (audio_config->playRate == 32000)
  {
    src32k_init();
    mix_frames = NUM_32KFRAMES;
  }
  else
  {
    mix_frames = NUM_FRAMES;
  }
  debug_printf("mix_frames = %d\n", mix_frames);
  mixInfo->rate = audio_config->pseudoRate;

  pDriver->Init(audio_config);
  soundLockId = pDriver->soundLockId = osMutexNew(&attributes_sound_lock);

  mixInfo->mixevqId = osMessageQueueNew(MIX_EV_DEPTH, sizeof(MIXCONTROL_EVENT), &attributes_mixevq);

  /* We'll keep sending contents of FinalSoundBuffer using DMA */

  pDriver->Start(audio_config);

  while (1)
  {
    MIXCONTROL_EVENT ctrl;

    osMessageQueueGet(mixInfo->mixevqId, &ctrl, 0, osWaitForever);

    switch (ctrl.event)
    {
    case MIX_PLAY:		// Start playing specified FLAC file
      if (mixInfo->state != MIX_ST_IDLE)
      {
        debug_printf("MIX_PLAY: Bad state\n");
        osDelay(10*1000);
      }

      pflac = drflac_open_fatfile(ctrl.arg, &allocationCallbacks);
      mixInfo->ppos = mixInfo->psec = 0;
      fft_count = 0;
      flacInfo->loop_count = ctrl.option;
      if (flacInfo->loop_count > 0)
        flacInfo->loop_count--;

      memset(audio_buffer, 0, sizeof(audio_buffer));
      fftInfo->getptr = fftInfo->putptr = audio_buffer;
      fftInfo->samples = 0;
      arm_rfft_fast_init_f32(&fft_instance, FFT_SAMPLES);

      pmusic = MusicFrameBuffer;

      if (pflac)
      {
        /* Read two frames of music data into MusicFrameBuffer. */
        num_read = drflac_read_pcm_frames_s16(pflac, NUM_FRAMES * BUF_FACTOR, (drflac_int16 *)MusicFrameBuffer);
        if (num_read > 0)
        {
          int res;

          flacInfo->pcm_pos += num_read;
          if (argval & MIXER_FFT_ENABLE)
          {
            res = process_fft(fftInfo, MusicFrameBuffer, NUM_FRAMES * BUF_FACTOR);
            if (res)
            {
              guiev.evcode = GUIEV_FFT_UPDATE;
              guiev.evval0 = fft_count;
              postGuiEvent(&guiev);
              fft_count++;
            }
          }
          mixInfo->state = MIX_ST_PLAY_REQ;

          if (audio_config->playRate == 32000)
          {
            /* For 32K audio (DUALSHOCK), convert music data to 32K sampling. */
            src32k_process(MusicFrameBuffer, NUM_FRAMES * 2);
          }
        }
      }
      else
      {
          debug_printf("FLAC open failed\n");
      }
      break;
    case MIX_DATA_REQ:
      pmusic = MusicFrameBuffer;

      switch (mixInfo->state)
      {
      case MIX_ST_PLAY_REQ:
         /* In this state, MusicFrameBuffer has been completely filled up. */
        if (ctrl.option == 0)
        {
          /* First half of output buffer has became available.
           * Fill that space with music and sound data.
           */
          pDriver->MixSound(audio_config, pmusic, mix_frames);
          mixInfo->state = MIX_ST_PLAY;
          mixInfo->ppos += NUM_FRAMES;
        }
        else
        {
          /* Latter half of output buffer has drained.
           * Send silient data to the buffer and wait for first half buffer space
           * becames available.
           */
          pDriver->MixSound(audio_config, SilentBuffer, mix_frames);
        }
        break;
      case MIX_ST_PLAY:
        mixInfo->ppos += NUM_FRAMES;
        if (ctrl.option == 0)
        {
          /* If we need to loop back, seek to start position. */
          if ((flacInfo->loop_count != 0) && (flacInfo->pcm_pos >= flacInfo->loop_end))
          {
            flacInfo->pcm_pos = flacInfo->loop_start;
            drflac_seek_to_pcm_frame(pflac, flacInfo->pcm_pos);
          }
        }
        else
        {
          pmusic += NUM_FRAMES;
        }
#ifdef MIX_DEBUG
debug_printf("mix2: pmusic = %x\n", pmusic);
#endif
        /* Read next music data into free space. */
        num_read = drflac_read_pcm_frames_s16(pflac, NUM_FRAMES, (drflac_int16 *)pmusic);
#ifdef MIX_DEBUG
debug_printf("num_read = %d\n", num_read);
#endif
        if (num_read > 0)
        {
          flacInfo->pcm_pos += num_read;
          pmusic += num_read;
          /* If read data amount is less than NUM_FRAMES,
           * fill with silent data.
           */
          while (num_read < NUM_FRAMES)
          {
            pmusic->ch0 = 0;
            pmusic->ch1 = 0;
            pmusic++;
            num_read++;
          }
          if (argval & MIXER_FFT_ENABLE)
          {
            pmusic = MusicFrameBuffer;
            if (ctrl.option)
              pmusic += NUM_FRAMES;
            if (process_fft(fftInfo, pmusic, NUM_FRAMES))
            {
              guiev.evcode = GUIEV_FFT_UPDATE;
              guiev.evval0 = fft_count;
              postGuiEvent(&guiev);
              fft_count++;
            }
          }
          if (audio_config->playRate == 32000)
          {
            /* For 32K audio (DUALSHOCK), convert music data to 32K sampling. */
            src32k_process((ctrl.option == 0)?  MusicFrameBuffer : MusicFrameBuffer + NUM_FRAMES, NUM_FRAMES);
          }
        }
        else
        {
          if (flacInfo->loop_count)
          {
            if (flacInfo->loop_count > 0)
              flacInfo->loop_count--;
            flacInfo->pcm_pos = flacInfo->loop_start;
            drflac_seek_to_pcm_frame(pflac, flacInfo->pcm_pos);
          }
          else
          {
            GUI_EVENT guiev;
  
            mixInfo->state = MIX_ST_IDLE;
            mixInfo->idle_count = IDLE_PERIOD;

            if (argval & MIXER_FFT_ENABLE)
            {
              guiev.evcode = GUIEV_MUSIC_FINISH;
              postGuiEvent(&guiev);
            }
          }
        }
        pmusic = MusicFrameBuffer;
        if (ctrl.option)
          pmusic += NUM_FRAMES;
        pDriver->MixSound(audio_config, pmusic, mix_frames);
#ifdef MIX_DEBUG
debug_printf("Mix2 (%d), %d @ %d\n", ctrl.option, mixInfo->ppos, flacInfo->pcm_pos);
#endif
        break;
      case MIX_ST_IDLE:
        if (fftInfo->samples > 0)
        {
          if (argval & MIXER_FFT_ENABLE)
          {
            if (process_fft(fftInfo, (AUDIO_STEREO *)SilentBuffer, NUM_FRAMES))
            {
              guiev.evcode = GUIEV_FFT_UPDATE;
              guiev.evval0 = fft_count;
              postGuiEvent(&guiev);
              fft_count++;
            }
          }
          if (mixInfo->idle_count > 0)
          {
             mixInfo->idle_count--;
             if (mixInfo->idle_count == 0)
               fftInfo->samples = 0;
          }
        }
        pDriver->MixSound(audio_config, SilentBuffer, mix_frames);
        break;
      default:
        debug_printf("st = %d\n", mixInfo->state);
        break;
      }
      psec = mixInfo->ppos / audio_config->pseudoRate;
      if (psec > mixInfo->psec)
      {
        mixInfo->psec = psec;
        postGuiEventMessage(GUIEV_PSEC_UPDATE, psec, NULL, NULL);
      }
      break;
    case MIX_PAUSE:
#ifdef MIX_DEBUG
      debug_printf("MIX_PAUSE\n");
#endif
      mixInfo->state = MIX_ST_IDLE;
      mixInfo->idle_count = IDLE_PERIOD;
      break;
    case MIX_RESUME:
#ifdef MIX_DEBUG
      debug_printf("MIX_RESUME\n");
#endif
      mixInfo->state = MIX_ST_PLAY;
      break;
    case MIX_FFT_DISABLE:
      debug_printf("FFT_DISABLE\n");
      argval &= ~MIXER_FFT_ENABLE;
      memset(band_val, 0, sizeof(band_val));
      break;
    case MIX_FFT_ENABLE:
      argval |= MIXER_FFT_ENABLE;
      break;
    case MIX_HALT:
#ifdef MIX_DEBUG
      debug_printf("MIX_HALT\n");
#endif
      drflac_close_fatfs(pflac);
      mixInfo->state = MIX_ST_IDLE;
      mixInfo->idle_count = IDLE_PERIOD;
      break;
    case MIX_SET_VOLUME:
      debug_printf("SetVolume: %d\n", ctrl.arg);
      pDriver->SetVolume((int) ctrl.arg);
      break;
    default:
      debug_printf("event = %x\n", ctrl.event);
      break;
    }
  }
  pDriver->Stop(audio_config);
}

/*
 * Public functions to suppoort SDL_Mixer API.
 */

static MUSIC_STATE play_state;

void Mix_FFT_Enable()
{
  MIXCONTROL_EVENT mixc;

  mixc.event = MIX_FFT_ENABLE;
  osMessageQueuePut(MixInfo.mixevqId, &mixc, 0, 0);
}

void Mix_FFT_Disable()
{
  MIXCONTROL_EVENT mixc;

  mixc.event = MIX_FFT_DISABLE;
  osMessageQueuePut(MixInfo.mixevqId, &mixc, 0, 0);
}

int Mix_PlayMusic(Mix_Music *music, int loop)
{
  MIXCONTROL_EVENT mixc;
  int st;

  mixc.event = MIX_PLAY;
  mixc.arg = (void *)music->fname;
  mixc.option = loop;
  play_state = music->state = MST_PLAYING;
  if (music->magic != MIX_MAGIC)
     debug_printf("Bad magic.\n");
  st = osMessageQueuePut(MixInfo.mixevqId, &mixc, 0, 0);
  if (st != osOK)
    debug_printf("failed to put. %d\n", st);
  return 0;
}

int Mix_ResumeMusic(Mix_Music *music)
{
  MIXCONTROL_EVENT mixc;

  switch (music->state)
  {
  case MST_LOADED:
    Mix_PlayMusic(music, 0);
    break;
  case MST_PAUSED:
    mixc.event = MIX_RESUME;
    play_state = music->state = MST_PLAYING;
    osMessageQueuePut(MixInfo.mixevqId, &mixc, 0, 0);
    break;
  default:
    break;
  }
  return 0;
}

int Mix_PauseMusic(Mix_Music *music)
{
  MIXCONTROL_EVENT mixc;

  mixc.event = MIX_PAUSE;
  play_state = music->state = MST_PAUSED;
  if (music->magic != MIX_MAGIC)
     debug_printf("Bad magic.\n");
  osMessageQueuePut(MixInfo.mixevqId, &mixc, 0, 0);
  return 0;
}

void Mix_HaltMusic()
{
  MIXCONTROL_EVENT mixc;

  mixc.event = MIX_HALT;
  mixc.arg = NULL;
  play_state = MST_INIT;
  osMessageQueuePut(MixInfo.mixevqId, &mixc, 0, 0);
}

Mix_Music *Mix_LoadMUS(const char *file)
{
  Mix_Music *music;

  music = (Mix_Music *)malloc(sizeof(Mix_Music));
  music->magic = MIX_MAGIC;
  music->fname = file;
  play_state = music->state = MST_LOADED;
  return music;
}

int Mix_PlayingMusic()
{
  return (play_state == MST_PLAYING)? 1 : 0;
}

int Mix_VolumeMusic(int volume)
{
  MIXCONTROL_EVENT mixc;

  if (volume < 0)
    volume = 75;

  mixc.event = MIX_SET_VOLUME;
  mixc.arg = (void *)volume;
  osMessageQueuePut(MixInfo.mixevqId, &mixc, 0, 0);

  return volume;
}

void Mix_FreeMusic(Mix_Music *music)
{
  if (music )
  {
     if (music->magic != MIX_MAGIC)
       debug_printf("BAD MAGIC %x\n", music->magic);
     music->magic = 0;
     free(music);
  }
}

static osThreadId_t mixid;

void Start_SDLMixer(AUDIO_CONF *audio_conf)
{
  if (mixid == 0)
  {
    mixid = osThreadNew(StartMixPlayerTask, (void *)audio_conf, &attributes_mixplayer);
  }
}

void Start_Doom_SDLMixer()
{
  if (mixid == 0)
  {
    mixid = osThreadNew(StartMixPlayerTask, (void *)get_audio_conf(), &attributes_mixplayer);
  }
}

int Mix_Started()
{
  return (int)mixid;
}

void Stop_SDLMixer()
{
  osThreadTerminate(mixid);
  mixid = 0;
}

SECTION_DTCMRAM CHANINFO ChanInfo[NUM_CHANNELS];

#define	MIX_DEBUGx

int Mix_PlayChannel(int channel, Mix_Chunk *chunk, int loops)
{
#ifdef MIX_DEBUG
  debug_printf("%s: %d, %d\n", __FUNCTION__, channel, chunk->alen);
#endif
  osMutexAcquire(soundLockId, osWaitForever);
  ChanInfo[channel].chunk = chunk;
  ChanInfo[channel].loop = loops;
  ChanInfo[channel].flag |= FL_SET;
  ChanInfo[channel].pread = (AUDIO_STEREO *)chunk->abuf;
  ChanInfo[channel].plast = (AUDIO_STEREO *)(chunk->abuf + chunk->alen);
  osMutexRelease(soundLockId);
  return channel;
}

int Mix_PlayPosition(int channel)
{
  Mix_Chunk *chunk;
  AUDIO_CONF *aconf = get_audio_conf();
  int pos;

  osMutexAcquire(soundLockId, osWaitForever);
  chunk = ChanInfo[channel].chunk;
  pos = ChanInfo[channel].pread - (AUDIO_STEREO *)chunk->abuf;
  osMutexRelease(soundLockId);

  if (aconf->playRate == 32000)
    pos = pos * 3 / 2;
  return pos;
}

int Mix_LoadChannel(int channel, Mix_Chunk *chunk, int loops)
{
#ifdef MIX_DEBUG
  debug_printf("%s: %d, %d\n", __FUNCTION__, channel, chunk->alen);
#endif
  osMutexAcquire(soundLockId, osWaitForever);
  ChanInfo[channel].chunk = chunk;
  ChanInfo[channel].loop = loops;
  ChanInfo[channel].flag = 0;
  ChanInfo[channel].pread = (AUDIO_STEREO *)chunk->abuf;
  ChanInfo[channel].plast = (AUDIO_STEREO *)(chunk->abuf + chunk->alen);
  ChanInfo[channel].vol_left = MIX_MAX_VOLUME;
  ChanInfo[channel].vol_right = MIX_MAX_VOLUME;
  osMutexRelease(soundLockId);
  return channel;
}

int Mix_ResumeChannel(int channel)
{
  osMutexAcquire(soundLockId, osWaitForever);
  ChanInfo[channel].flag |= FL_SET|FL_PLAY;
  osMutexRelease(soundLockId);
  return channel;
}

int Mix_PauseChannel(int channel)
{
  osMutexAcquire(soundLockId, osWaitForever);
  ChanInfo[channel].flag &= ~FL_SET;
  osMutexRelease(soundLockId);
  return channel;
}

int Mix_HaltChannel(int channel)
{
#ifdef MIX_DEBUG
  debug_printf("%s: %d\n", __FUNCTION__, channel);
#endif

  osMutexAcquire(soundLockId, osWaitForever);
  if (channel >= 0)
  {
    ChanInfo[channel].flag &= ~(FL_PLAY);
  }
  else	/* Close all channels */
  {
    for (channel = 0; channel < NUM_CHANNELS; channel++)
      ChanInfo[channel].flag &= ~(FL_PLAY|FL_SET);
  }
  osMutexRelease(soundLockId);
  return 0;
}

int Mix_Playing(int channel)
{
  //debug_printf("%s %d:\n", __FUNCTION__, channel);
  if (ChanInfo[channel].flag & FL_SET)
    return 0;
  return 1;
}

int Mix_SetPanning(int channel, uint8_t left, uint8_t right)
{
  CHANINFO *cinfo = &ChanInfo[channel];
  int res = 0;

#ifdef MIX_DEBUG
  debug_printf("%s: %d (%d, %d)\n", __FUNCTION__, channel, left, right);
#endif
  osMutexAcquire(soundLockId, osWaitForever);
  if (cinfo->pread < cinfo->plast)
  {
    cinfo->vol_left = left;
    cinfo->vol_right = right;
    //cinfo->flag |= FL_PLAY;
    res = 1;
  }
  osMutexRelease(soundLockId);
  return res;
}

void Mix_CloseAudio()
{
  debug_printf("%ss:\n", __FUNCTION__);
}

int Mix_AllocateChannels(int chans)
{
  int i;

  debug_printf("%ss:\n", __FUNCTION__);

  osMutexAcquire(soundLockId, osWaitForever);
  for (i = 0; i < chans; i++)
    ChanInfo[i].flag = FL_ALLOCED;
  osMutexRelease(soundLockId);
  return 1;
}

int SDL_PauseAudio(int on)
{
  debug_printf("%ss:\n", __FUNCTION__);
  return 1;
}

void mix_request_data(int full)
{
  MIXCONTROL_EVENT evcode;
  int st;

  evcode.event = MIX_DATA_REQ;
  evcode.option = full;
  st = osMessageQueuePut(MixInfo.mixevqId, &evcode, 0, 0);
  if (st != osOK)
    debug_printf("request failed (%d)\n", st);
}

int Mix_QueryFreq()
{
  AUDIO_CONF *aconf;

  aconf = get_audio_conf();
  return aconf->playRate;
}
