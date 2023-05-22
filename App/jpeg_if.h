#ifndef _JPEG_IF_H
#define _JPEG_IF_H

#include "lvgl.h"

#define	SCREEN_DIR	"/Screen"

/* RGB Color format definition for JPEG encoding/Decoding : Should not be modified*/
#define JPEG_ARGB8888            0  /* ARGB8888 Color Format */
#define JPEG_RGB888              1  /* RGB888 Color Format   */
#define JPEG_RGB565              2  /* RGB565 Color Format   */

#define YCBCR_420_BLOCK_SIZE       384     /* YCbCr 4:2:0 MCU : 4 8x8 blocks of Y + 1 8x8 block of Cb + 1 8x8 block of Cr   */
#define YCBCR_422_BLOCK_SIZE       256     /* YCbCr 4:2:2 MCU : 2 8x8 blocks of Y + 1 8x8 block of Cb + 1 8x8 block of Cr   */
#define YCBCR_444_BLOCK_SIZE       192     /* YCbCr 4:4:4 MCU : 1 8x8 block of Y + 1 8x8 block of Cb + 1 8x8 block of Cr   */

#define JPEG_RGB_FORMAT JPEG_RGB565

#define	MAX_INPUT_LINES	16
#if 0
#define	MCU_BUFSIZE	(LCD_WIDTH/16*YCBCR_420_BLOCK_SIZE*(MAX_INPUT_LINES/16))
#else
#define	MCU_BUFSIZE	(LCD_WIDTH/8*YCBCR_444_BLOCK_SIZE*(MAX_INPUT_LINES/8))
#endif

#define JPEG_BUFFER_EMPTY 0
#define JPEG_BUFFER_FULL  1

typedef struct {
  JPEG_HandleTypeDef *hjpeg;
  JPEG_ConfTypeDef JPEG_Info;

  /* Decoder related variables */
  osSemaphoreId_t sem_encoderId; 
  osSemaphoreId_t sem_jpegbufId; 
  osEventFlagsId_t encoder_flagId;

  /* Encoder related variables */
  uint32_t MCU_TotalNb;
  uint8_t  *MCU_DataBuffer;
  volatile uint32_t Output_Is_Paused;
  volatile uint32_t Input_Is_Paused;
  uint8_t  *jpegOutAddress;
  volatile uint8_t  inbState;
  volatile uint8_t  outbState;
  uint32_t feed_unit;
} JPEGDEVINFO;

void init_jpeg();
void save_jpeg_start(int fnum, char *mode, int w, int h);
void save_jpeg_data(uint8_t *bp, int w, int h);
void save_jpeg_finish();
void save_jpeg_write_done();

#endif
