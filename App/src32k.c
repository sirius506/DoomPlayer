/**
 *  @brief Sampling Rate Convertion to 32KHz
 *
 *  Assuming 48KHz sampling rate, generates 32KHz sampling rate
 *  audio data. To accomplish this task, first we interpolate to
 *  twice, then decimate to one third.
 *  Real DOOM music and sound data is prepared as 44.1KHz based
 *  sampling rate, but here we treat them as 48KHz, since it is
 *  easier to generate 32KHz audio data.
 */
#include "DoomPlayer.h"
#include "mplayer.h"
#include "arm_math.h"
#include "src32k.h"

#define	INTERP_TAPS	28
static arm_fir_interpolate_instance_q15 linInstance;
static arm_fir_interpolate_instance_q15 rinInstance;
static q15_t linState[INTERP_TAPS/2+NUM_FRAMES-1];
static q15_t rinState[INTERP_TAPS/2+NUM_FRAMES-1];

#define	DECIM_TAPS	29
static arm_fir_decimate_instance_q15 loutInstance;
static arm_fir_decimate_instance_q15 routInstance;
static q15_t loutState[NUM_FRAMES*2+DECIM_TAPS-1];
static q15_t routState[NUM_FRAMES*2+DECIM_TAPS-1];

const q15_t CoeffsDouble[INTERP_TAPS] =
{
 0xffcd, 0x0044, 0xff96, 0x00aa, 0xfef9, 0x0181,
 0xfded, 0x02b9, 0xfc96, 0x041a, 0xfb40, 0x054f,
 0xfa43, 0x0602, 0x7a15, 0x0602, 0xfa43, 0x054f,
 0xfb40, 0x041a, 0xfc96, 0x02b9, 0xfded, 0x0181,
 0xfef9, 0x00aa, 0xff96, 0x0044, 
/* 0xffcd, */
};

const q15_t CoeffsOneThird[DECIM_TAPS] =
{
 0xffea, 0x0028, 0xffb3, 0x008e, 0xff0c, 0x0183,
 0xfdc6, 0x0315, 0xfbf8, 0x0503, 0xfa0b, 0x06ca,
 0xf88f, 0x07dc, 0x780f, 0x07dc, 0xf88f, 0x06ca,
 0xfa0b, 0x0503, 0xfbf8, 0x0315, 0xfdc6, 0x0183,
 0xff0c, 0x008e, 0xffb3, 0x0028, 0xffea,
};

/*
 * @brief Initializa conversion filters.
 */
void src32k_init()
{
    arm_fir_interpolate_init_q15(&linInstance, 2, INTERP_TAPS, (const q15_t *)&CoeffsDouble, linState, NUM_FRAMES);
    arm_fir_interpolate_init_q15(&rinInstance, 2, INTERP_TAPS, (const q15_t *)&CoeffsDouble, rinState, NUM_FRAMES);
    arm_fir_decimate_init_q15(&loutInstance, DECIM_TAPS, 3, (const q15_t *)&CoeffsOneThird, loutState, NUM_FRAMES*2);
    arm_fir_decimate_init_q15(&routInstance, DECIM_TAPS, 3, (const q15_t *)&CoeffsOneThird, routState, NUM_FRAMES*2);
}

static q15_t left_work[NUM_FRAMES];
static q15_t right_work[NUM_FRAMES];
static q15_t interp_buffer[NUM_FRAMES*2];

/*
 * @brief Convert 48KHz 16bit PCM data into 32KHz sampling.
 *
 * @param[in,out] bp pointer to 16bit PCM data
 * @param[in] flen Number of PCM samples
 * @return Number of generated samples
 */
int src32k_pcm_mono(int16_t *bp, int flen)
{
  int blen, olen, ototal;
  int16_t *outp;

  outp = bp;
  ototal = olen = 0;

  while (flen > 0)
  {
    blen = (flen > NUM_FRAMES)? NUM_FRAMES : flen;
    arm_fir_interpolate_q15(&linInstance, bp, interp_buffer, blen);
    arm_fir_decimate_q15(&loutInstance, interp_buffer, outp, blen*2);
    bp += blen;
    flen -= blen;
    olen = blen * 2 / 3;
    ototal += olen;
    outp += olen;
  }
  return ototal;
}

#ifdef CONV_LATER
int src32k_pcm(AUDIO_STEREO *pcm, int flen)
{
  int blen, olen, ototal;
  int i;
  AUDIO_STEREO *outp;

  outp = pcm;
  ototal = olen = 0;

  while (flen > 0)
  {
    blen = (flen > NUM_FRAMES)? NUM_FRAMES : flen;

    for (i = 0; i < blen; i++)
    {
      left_work[i] = pcm[i].ch0;
    }
    arm_fir_interpolate_q15(&linInstance, left_work, interp_buffer, blen);
    arm_fir_decimate_q15(&loutInstance, interp_buffer, left_work, blen*2);

    olen = blen * 2 / 3;

    for (i = 0; i < olen; i++)
    {
      outp[i].ch0 = outp[i].ch1 = left_work[i];
    }

    flen -= blen;
    ototal += olen;
    pcm += blen;
    outp += olen;
  }
  return ototal;
}
#endif

/**
 * @brief Convert 48KHz sampling stereo audio data to 32KHz sampling.
 * @param[in,out] psrc pointer to audio data.
 * @param[in] in_frames Number of input stereo data frames.
 * @returns Number of generated stereo data frames.
 */
int src32k_process(AUDIO_STEREO *psrc, int in_frames)
{
  int i;
  int num_frame;

  if (in_frames != NUM_FRAMES)
    return 0;

  for (i = 0; i < in_frames; i++)
  {
      left_work[i] = psrc[i].ch0;
      right_work[i] = psrc[i].ch1;
  }
  arm_fir_interpolate_q15(&linInstance, left_work, interp_buffer, NUM_FRAMES);
  arm_fir_decimate_q15(&loutInstance, interp_buffer, left_work, NUM_FRAMES*2);
  arm_fir_interpolate_q15(&rinInstance, right_work, interp_buffer, NUM_FRAMES);
  arm_fir_decimate_q15(&routInstance, interp_buffer, right_work, NUM_FRAMES*2);

  num_frame = in_frames * 2 / 3;
  for (i = 0; i < num_frame; i++)
  {
      psrc[i].ch0 = left_work[i];
      psrc[i].ch1 = right_work[i];
  }
  return num_frame;
}
