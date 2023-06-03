#ifndef _SRC32K_H_
#define _SRC32K_H_

void src32k_init();
int src32k_pcm_mono(int16_t *bp, int flen);
int src32k_process(AUDIO_STEREO *psrc, int in_frames);
#endif
