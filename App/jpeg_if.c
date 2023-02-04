#include "DoomPlayer.h"
#include <stdlib.h>
#include "lvgl.h"
#include "jpeg_if.h"
#include "app_task.h"
#include "jpeg_lut.h"

#define DO_POST

#define	JPEG_OBSIZE	(20*1024)

#define JPEG_GREEN_OFFSET        5       /* Offset of the GREEN color in a pixel         */    
#define JPEG_BYTES_PER_PIXEL     2       /* Number of bytes in a pixel                   */
#define JPEG_RGB565_GREEN_MASK   0x07E0  /* Mask of Green component in RGB565 Format     */

#if (JPEG_SWAP_RB == 0)
    #define JPEG_RED_OFFSET        11      /* Offset of the RED color in a pixel           */
    #define JPEG_BLUE_OFFSET       0       /* Offset of the BLUE color in a pixel          */
    #define JPEG_RGB565_RED_MASK   0xF800  /* Mask of Red component in RGB565 Format       */
    #define JPEG_RGB565_BLUE_MASK  0x001F  /* Mask of Blue component in RGB565 Format      */
#else
    #define JPEG_RED_OFFSET        0       /* Offset of the RED color in a pixel           */
    #define JPEG_BLUE_OFFSET       11      /* Offset of the BLUE color in a pixel          */
    #define JPEG_RGB565_RED_MASK   0x001F  /* Mask of Red component in RGB565 Format       */
    #define JPEG_RGB565_BLUE_MASK  0xF800  /* Mask of Blue component in RGB565 Format      */
#endif /* JPEG_SWAP_RB */

JPEGDEVINFO JpegDeviceInfo;

EVFLAG_DEF(encoderFlag)

typedef enum {
  ENCODER_DATAREQ = (1<<0),
  ENCODER_DATAREADY = (1<<1),
  ENCODER_COMPLETE = (1<<2),
} ENCORDER_FLAG;

typedef   uint32_t (* JPEG_RGBToYCbCr_Convert_Function)(uint8_t *pInBuffer, 
                                      uint8_t *pOutBuffer,
                                      uint32_t BlockIndex,
                                      uint32_t DataCount,
                                      uint32_t *ConvertedDataCount );

typedef struct __JPEG_MCU_RGB_ConvertorTypeDef
{
  uint32_t ColorSpace;
  uint32_t ChromaSubsampling;
  
  uint32_t ImageWidth;
  uint32_t ImageHeight;
  uint32_t ImageSize_Bytes;

  uint32_t LineOffset;
  uint32_t BlockSize;
  
  uint32_t H_factor;
  uint32_t V_factor;

  uint32_t WidthExtend;
  uint32_t ScaledWidth;
  
  uint32_t MCU_Total_Nb;
  
  const uint16_t *Y_MCU_LUT;  
  const uint16_t *Cb_MCU_LUT;  
  const uint16_t *Cr_MCU_LUT;  
  const uint16_t *K_MCU_LUT;

}JPEG_MCU_RGB_ConvertorTypeDef;

JPEG_HandleTypeDef    JPEG_Handle;

JPEG_RGBToYCbCr_Convert_Function pRGBToYCbCr_Convert_Function;

static JPEG_MCU_RGB_ConvertorTypeDef JPEG_ConvertorParams;

SECTION_AHBSRAM uint8_t MCU_Buffer[MCU_BUFSIZE];

SEMAPHORE_DEF(encoder_sem)
SEMAPHORE_DEF(jpegbuf_sem)

/**
  * @brief  Convert RGB to YCbCr 4:2:0 blocks pixels  
  * @param  pInBuffer  : pointer to input RGB888/ARGB8888 frame buffer.
  * @param  pOutBuffer : pointer to output YCbCr blocks buffer.
  * @param  BlockIndex : index of the input buffer first block in the final image.
  * @param  DataCount  : number of bytes in the input buffer .
  * @param  ConvertedDataCount  : number of converted bytes from input buffer.  
  * @retval Number of blcoks converted from RGB to YCbCr
  */
static uint32_t JPEG_ARGB_MCU_YCbCr420_ConvertBlocks (uint8_t *pInBuffer, 
                                      uint8_t *pOutBuffer, 
                                      uint32_t BlockIndex,
                                      uint32_t DataCount,
                                      uint32_t *ConvertedDataCount)
{  
  uint32_t numberMCU;
  uint32_t i,j, currentMCU, xRef,yRef, colones;

  uint32_t refline;
  int32_t ycomp, crcomp, cbcomp, offset;
  
  uint32_t red, green, blue;
  
  uint8_t *pOutAddr;
  uint8_t *pInAddr;
  

  numberMCU = ((3 * DataCount) / ( 2 * JPEG_BYTES_PER_PIXEL * YCBCR_420_BLOCK_SIZE));

  currentMCU = BlockIndex;
  *ConvertedDataCount = numberMCU * JPEG_ConvertorParams.BlockSize;

  pOutAddr = &pOutBuffer[0];

  while(currentMCU < (numberMCU + BlockIndex))
  {
    xRef = ((currentMCU *JPEG_ConvertorParams.H_factor) / JPEG_ConvertorParams.WidthExtend)*JPEG_ConvertorParams.V_factor;
    
    yRef = ((currentMCU *JPEG_ConvertorParams.H_factor) % JPEG_ConvertorParams.WidthExtend);

    
    refline = JPEG_ConvertorParams.ScaledWidth * xRef + (JPEG_BYTES_PER_PIXEL*yRef);

    currentMCU++;
    
    if(((currentMCU *JPEG_ConvertorParams.H_factor) % JPEG_ConvertorParams.WidthExtend) == 0)
    {
      colones = JPEG_ConvertorParams.H_factor - JPEG_ConvertorParams.LineOffset;
    }
    else
    {
      colones = JPEG_ConvertorParams.H_factor;
    }    
    offset = 0;

    for(i= 0; i <  JPEG_ConvertorParams.V_factor; i+=2)
    {
      
      pInAddr = &pInBuffer[0] ;
      
      for(j=0; j < colones; j+=2)
      {
        /* First Pixel */
#if (JPEG_RGB_FORMAT == JPEG_RGB565)
        red   = (((*(__IO uint16_t *)(pInAddr + refline)) & JPEG_RGB565_RED_MASK)   >> JPEG_RED_OFFSET) ;
        green = (((*(__IO uint16_t *)(pInAddr + refline)) & JPEG_RGB565_GREEN_MASK) >> JPEG_GREEN_OFFSET) ;
        blue  = (((*(__IO uint16_t *)(pInAddr + refline)) & JPEG_RGB565_BLUE_MASK)  >> JPEG_BLUE_OFFSET) ;
        red   = (red << 3)   | (red >> 2);
        green = (green << 2) | (green >> 4);
        blue  = (blue << 3)  | (blue >> 2);
#else
        red   = (*(pInAddr + refline + JPEG_RED_OFFSET/8)) ;
        green = (*(pInAddr + refline + JPEG_GREEN_OFFSET/8)) ;
        blue  = (*(pInAddr + refline + JPEG_BLUE_OFFSET/8)) ;
#endif
        ycomp  = (int32_t)(*(RED_Y_LUT + red)) + (int32_t)(*(GREEN_Y_LUT + green)) + (int32_t)(*(BLUE_Y_LUT + blue));
        cbcomp = (int32_t)(*(RED_CB_LUT + red)) + (int32_t)(*(GREEN_CB_LUT + green)) + (int32_t)(*(BLUE_CB_RED_CR_LUT + blue)) + 128;
        crcomp = (int32_t)(*(BLUE_CB_RED_CR_LUT + red)) + (int32_t)(*(GREEN_CR_LUT + green)) + (int32_t)(*(BLUE_CR_LUT + blue)) + 128;
        
        (*(pOutAddr + JPEG_ConvertorParams.Y_MCU_LUT[offset]))  = (ycomp);
        (*(pOutAddr + JPEG_ConvertorParams.Cb_MCU_LUT[offset])) = (cbcomp);
        (*(pOutAddr + JPEG_ConvertorParams.Cr_MCU_LUT[offset])) = (crcomp);
        
        /* Second Pixel */
#if (JPEG_RGB_FORMAT == JPEG_RGB565)
        red   = (((*(__IO uint16_t *)(pInAddr + JPEG_BYTES_PER_PIXEL + refline)) & JPEG_RGB565_RED_MASK)   >> JPEG_RED_OFFSET) ;
        green = (((*(__IO uint16_t *)(pInAddr + JPEG_BYTES_PER_PIXEL + refline)) & JPEG_RGB565_GREEN_MASK) >> JPEG_GREEN_OFFSET) ;
        blue  = (((*(__IO uint16_t *)(pInAddr + JPEG_BYTES_PER_PIXEL + refline)) & JPEG_RGB565_BLUE_MASK)  >> JPEG_BLUE_OFFSET) ;
        red   = (red << 3)   | (red >> 2);
        green = (green << 2) | (green >> 4);
        blue  = (blue << 3)  | (blue >> 2);
#else
        red   = (*(pInAddr + refline + JPEG_BYTES_PER_PIXEL + JPEG_RED_OFFSET/8)) ;
        green = (*(pInAddr + refline + JPEG_BYTES_PER_PIXEL + JPEG_GREEN_OFFSET/8)) ;
        blue  = (*(pInAddr + refline + JPEG_BYTES_PER_PIXEL + JPEG_BLUE_OFFSET/8)) ;
#endif  
        ycomp  = (int32_t)(*(RED_Y_LUT + red)) + (int32_t)(*(GREEN_Y_LUT + green)) + (int32_t)(*(BLUE_Y_LUT + blue));
        (*(pOutAddr + JPEG_ConvertorParams.Y_MCU_LUT[offset + 1]))  = (ycomp);
        
        /* Third Pixel */
#if (JPEG_RGB_FORMAT == JPEG_RGB565)
        red   = (((*(__IO uint16_t *)(pInAddr + JPEG_ConvertorParams.ScaledWidth + refline)) & JPEG_RGB565_RED_MASK)   >> JPEG_RED_OFFSET) ;
        green = (((*(__IO uint16_t *)(pInAddr + JPEG_ConvertorParams.ScaledWidth + refline)) & JPEG_RGB565_GREEN_MASK) >> JPEG_GREEN_OFFSET) ;
        blue  = (((*(__IO uint16_t *)(pInAddr + JPEG_ConvertorParams.ScaledWidth + refline)) & JPEG_RGB565_BLUE_MASK)  >> JPEG_BLUE_OFFSET) ;
        red   = (red << 3)   | (red >> 2);
        green = (green << 2) | (green >> 4);
        blue  = (blue << 3)  | (blue >> 2);
#else
        red   = (*(pInAddr + refline + JPEG_ConvertorParams.ScaledWidth + JPEG_RED_OFFSET/8)) ;
        green = (*(pInAddr + refline + JPEG_ConvertorParams.ScaledWidth + JPEG_GREEN_OFFSET/8)) ;
        blue  = (*(pInAddr + refline + JPEG_ConvertorParams.ScaledWidth + JPEG_BLUE_OFFSET/8)) ;
#endif
        ycomp  = (int32_t)(*(RED_Y_LUT + red)) + (int32_t)(*(GREEN_Y_LUT + green)) + (int32_t)(*(BLUE_Y_LUT + blue));
        
        (*(pOutAddr + JPEG_ConvertorParams.Y_MCU_LUT[offset + JPEG_ConvertorParams.H_factor]))  = (ycomp);
        
        /* Fourth Pixel */
#if (JPEG_RGB_FORMAT == JPEG_RGB565)
        red   = (((*(__IO uint16_t *)(pInAddr + refline + JPEG_ConvertorParams.ScaledWidth + JPEG_BYTES_PER_PIXEL)) & JPEG_RGB565_RED_MASK)   >> JPEG_RED_OFFSET) ;
        green = (((*(__IO uint16_t *)(pInAddr + refline + JPEG_ConvertorParams.ScaledWidth + JPEG_BYTES_PER_PIXEL)) & JPEG_RGB565_GREEN_MASK) >> JPEG_GREEN_OFFSET) ;
        blue  = (((*(__IO uint16_t *)(pInAddr + refline + JPEG_ConvertorParams.ScaledWidth + JPEG_BYTES_PER_PIXEL)) & JPEG_RGB565_BLUE_MASK)  >> JPEG_BLUE_OFFSET) ;
        red   = (red << 3)   | (red >> 2);
        green = (green << 2) | (green >> 4);
        blue  = (blue << 3)  | (blue >> 2);
#else
        red   = (*(pInAddr + refline + JPEG_ConvertorParams.ScaledWidth + JPEG_BYTES_PER_PIXEL + JPEG_RED_OFFSET/8)) ;
        green = (*(pInAddr + refline + JPEG_ConvertorParams.ScaledWidth + JPEG_BYTES_PER_PIXEL + JPEG_GREEN_OFFSET/8)) ;
        blue  = (*(pInAddr + refline + JPEG_ConvertorParams.ScaledWidth + JPEG_BYTES_PER_PIXEL + JPEG_BLUE_OFFSET/8)) ;
#endif
        ycomp  = (int32_t)(*(RED_Y_LUT + red)) + (int32_t)(*(GREEN_Y_LUT + green)) + (int32_t)(*(BLUE_Y_LUT + blue));
        (*(pOutAddr + JPEG_ConvertorParams.Y_MCU_LUT[offset + JPEG_ConvertorParams.H_factor + 1]))  = (ycomp);
        
        /****************/
        
        pInAddr += JPEG_BYTES_PER_PIXEL * 2;
        offset+=2;
      }
      offset += JPEG_ConvertorParams.H_factor + (JPEG_ConvertorParams.H_factor - colones);
      refline += JPEG_ConvertorParams.ScaledWidth * 2 ;
    }
    pOutAddr +=  JPEG_ConvertorParams.BlockSize ;
  }
  return numberMCU;
}

/**
  * @brief  Convert RGB to YCbCr 4:2:2 blocks pixels  
  * @param  pInBuffer  : pointer to input RGB888/ARGB8888 frame buffer.
  * @param  pOutBuffer : pointer to output YCbCr blocks buffer.
  * @param  BlockIndex : index of the input buffer first block in the final image.
  * @param  DataCount  : number of bytes in the input buffer .
  * @param  ConvertedDataCount  : number of converted bytes from input buffer.  
  * @retval Number of blcoks converted from RGB to YCbCr
  */
static uint32_t JPEG_ARGB_MCU_YCbCr422_ConvertBlocks (uint8_t *pInBuffer, 
                                      uint8_t *pOutBuffer, 
                                      uint32_t BlockIndex,
                                      uint32_t DataCount,
                                      uint32_t *ConvertedDataCount)
{  
  uint32_t numberMCU;
  uint32_t i,j, currentMCU, xRef,yRef, colones;

  uint32_t refline;
  int32_t ycomp, crcomp, cbcomp, offset;
  
  uint32_t red, green, blue;
  
  uint8_t *pOutAddr;
  uint8_t *pInAddr;

  numberMCU = ((2 * DataCount) / (JPEG_BYTES_PER_PIXEL * YCBCR_422_BLOCK_SIZE));

  currentMCU = BlockIndex;
  *ConvertedDataCount = numberMCU * JPEG_ConvertorParams.BlockSize;

  pOutAddr = &pOutBuffer[0];

  while(currentMCU < (numberMCU + BlockIndex))
  {
    xRef = ((currentMCU *JPEG_ConvertorParams.H_factor) / JPEG_ConvertorParams.WidthExtend)*JPEG_ConvertorParams.V_factor;
    
    yRef = ((currentMCU *JPEG_ConvertorParams.H_factor) % JPEG_ConvertorParams.WidthExtend);

    
    refline = JPEG_ConvertorParams.ScaledWidth * xRef + (JPEG_BYTES_PER_PIXEL*yRef);

    currentMCU++;
    
    if(((currentMCU *JPEG_ConvertorParams.H_factor) % JPEG_ConvertorParams.WidthExtend) == 0)
    {
      colones = JPEG_ConvertorParams.H_factor - JPEG_ConvertorParams.LineOffset;
    }
    else
    {
      colones = JPEG_ConvertorParams.H_factor;
    }    
    offset = 0;

    for(i= 0; i <  JPEG_ConvertorParams.V_factor; i+=1)
    {
      
      pInAddr = &pInBuffer[0] ;
      
      for(j=0; j < colones; j+=2)
      {
        /* First Pixel */
#if (JPEG_RGB_FORMAT == JPEG_RGB565)
        red   = (((*(__IO uint16_t *)(pInAddr + refline)) & JPEG_RGB565_RED_MASK)   >> JPEG_RED_OFFSET) ;
        green = (((*(__IO uint16_t *)(pInAddr + refline)) & JPEG_RGB565_GREEN_MASK) >> JPEG_GREEN_OFFSET) ;
        blue  = (((*(__IO uint16_t *)(pInAddr + refline)) & JPEG_RGB565_BLUE_MASK)  >> JPEG_BLUE_OFFSET) ;
        red   = (red << 3)   | (red >> 2);
        green = (green << 2) | (green >> 4);
        blue  = (blue << 3)  | (blue >> 2);
#else
        red   = (*(pInAddr + refline + JPEG_RED_OFFSET/8)) ;
        green = (*(pInAddr + refline + JPEG_GREEN_OFFSET/8)) ;
        blue  = (*(pInAddr + refline + JPEG_BLUE_OFFSET/8)) ;
#endif
        ycomp  = (int32_t)(*(RED_Y_LUT + red)) + (int32_t)(*(GREEN_Y_LUT + green)) + (int32_t)(*(BLUE_Y_LUT + blue));
        cbcomp = (int32_t)(*(RED_CB_LUT + red)) + (int32_t)(*(GREEN_CB_LUT + green)) + (int32_t)(*(BLUE_CB_RED_CR_LUT + blue)) + 128;
        crcomp = (int32_t)(*(BLUE_CB_RED_CR_LUT + red)) + (int32_t)(*(GREEN_CR_LUT + green)) + (int32_t)(*(BLUE_CR_LUT + blue)) + 128;
        
        (*(pOutAddr + JPEG_ConvertorParams.Y_MCU_LUT[offset]))  = ycomp;
        (*(pOutAddr + JPEG_ConvertorParams.Cb_MCU_LUT[offset])) = cbcomp;
        (*(pOutAddr + JPEG_ConvertorParams.Cr_MCU_LUT[offset])) = crcomp;
        
        /* Second Pixel */
#if (JPEG_RGB_FORMAT == JPEG_RGB565)
        red   = (((*(__IO uint16_t *)(pInAddr + refline + JPEG_BYTES_PER_PIXEL)) & JPEG_RGB565_RED_MASK)   >> JPEG_RED_OFFSET) ;
        green = (((*(__IO uint16_t *)(pInAddr + refline + JPEG_BYTES_PER_PIXEL)) & JPEG_RGB565_GREEN_MASK) >> JPEG_GREEN_OFFSET) ;
        blue  = (((*(__IO uint16_t *)(pInAddr + refline + JPEG_BYTES_PER_PIXEL)) & JPEG_RGB565_BLUE_MASK)  >> JPEG_BLUE_OFFSET) ;
        red   = (red << 3)   | (red >> 2);
        green = (green << 2) | (green >> 4);
        blue  = (blue << 3)  | (blue >> 2);
#else
        red   = (*(pInAddr + refline + JPEG_BYTES_PER_PIXEL + JPEG_RED_OFFSET/8)) ;
        green = (*(pInAddr + refline + JPEG_BYTES_PER_PIXEL + JPEG_GREEN_OFFSET/8)) ;
        blue  = (*(pInAddr + refline + JPEG_BYTES_PER_PIXEL + JPEG_BLUE_OFFSET/8)) ;
#endif
        ycomp  = (int32_t)(*(RED_Y_LUT + red)) + (int32_t)(*(GREEN_Y_LUT + green)) + (int32_t)(*(BLUE_Y_LUT + blue));
        (*(pOutAddr + JPEG_ConvertorParams.Y_MCU_LUT[offset + 1]))  = ycomp;
        
        /****************/
        
        pInAddr += JPEG_BYTES_PER_PIXEL * 2;
        offset+=2;
      }
      offset += (JPEG_ConvertorParams.H_factor - colones);
      refline += JPEG_ConvertorParams.ScaledWidth ;
      
    }
    pOutAddr +=  JPEG_ConvertorParams.BlockSize;
  }

  return numberMCU;
}

/**
  * @brief  Convert RGB to YCbCr 4:4:4 blocks pixels  
  * @param  pInBuffer  : pointer to input RGB888/ARGB8888 frame buffer.
  * @param  pOutBuffer : pointer to output YCbCr blocks buffer.
  * @param  BlockIndex : index of the input buffer first block in the final image.
  * @param  DataCount  : number of bytes in the input buffer .
  * @param  ConvertedDataCount  : number of converted bytes from input buffer.  
  * @retval Number of blcoks converted from RGB to YCbCr
  */
static uint32_t JPEG_ARGB_MCU_YCbCr444_ConvertBlocks (uint8_t *pInBuffer, 
                                      uint8_t *pOutBuffer, 
                                      uint32_t BlockIndex,
                                      uint32_t DataCount,
                                      uint32_t *ConvertedDataCount)
{  
  uint32_t numberMCU;
  uint32_t i,j, currentMCU, xRef,yRef, colones;

  uint32_t refline;
  int32_t ycomp, crcomp, cbcomp, offset;
  
  uint32_t red, green, blue;
  
  uint8_t *pOutAddr;
  uint8_t *pInAddr;

  numberMCU = ((3 * DataCount) / (JPEG_BYTES_PER_PIXEL * YCBCR_444_BLOCK_SIZE));

  currentMCU = BlockIndex;
  *ConvertedDataCount = numberMCU * JPEG_ConvertorParams.BlockSize;

  pOutAddr = &pOutBuffer[0];

  while(currentMCU < (numberMCU + BlockIndex))
  {
    xRef = ((currentMCU *JPEG_ConvertorParams.H_factor) / JPEG_ConvertorParams.WidthExtend)*JPEG_ConvertorParams.V_factor;
    
    yRef = ((currentMCU *JPEG_ConvertorParams.H_factor) % JPEG_ConvertorParams.WidthExtend);
    
    refline = JPEG_ConvertorParams.ScaledWidth * xRef + (JPEG_BYTES_PER_PIXEL*yRef);

    currentMCU++;
    
    if(((currentMCU *JPEG_ConvertorParams.H_factor) % JPEG_ConvertorParams.WidthExtend) == 0)
    {
      colones = JPEG_ConvertorParams.H_factor - JPEG_ConvertorParams.LineOffset;
    }
    else
    {
      colones = JPEG_ConvertorParams.H_factor;
    }    
    offset = 0;

    for(i= 0; i <  JPEG_ConvertorParams.V_factor; i++)
    {
      pInAddr = &pInBuffer[0] ;
      
      for(j=0; j < colones; j++)
      {
#if (JPEG_RGB_FORMAT == JPEG_RGB565)
        red   = (((*(__IO uint16_t *)(pInAddr + refline)) & JPEG_RGB565_RED_MASK)   >> JPEG_RED_OFFSET) ;
        green = (((*(__IO uint16_t *)(pInAddr + refline)) & JPEG_RGB565_GREEN_MASK) >> JPEG_GREEN_OFFSET) ;
        blue  = (((*(__IO uint16_t *)(pInAddr + refline)) & JPEG_RGB565_BLUE_MASK)  >> JPEG_BLUE_OFFSET) ;
        red   = (red << 3)   | (red >> 2);
        green = (green << 2) | (green >> 4);
        blue  = (blue << 3)  | (blue >> 2);
#else
        red   = (*(pInAddr + refline + JPEG_RED_OFFSET/8)) ;
        green = (*(pInAddr + refline + JPEG_GREEN_OFFSET/8)) ;
        blue  = (*(pInAddr + refline + JPEG_BLUE_OFFSET/8)) ;
#endif
        ycomp  = (int32_t)(*(RED_Y_LUT + red)) + (int32_t)(*(GREEN_Y_LUT + green)) + (int32_t)(*(BLUE_Y_LUT + blue));
        cbcomp = (int32_t)(*(RED_CB_LUT + red)) + (int32_t)(*(GREEN_CB_LUT + green)) + (int32_t)(*(BLUE_CB_RED_CR_LUT + blue)) + 128;
        crcomp = (int32_t)(*(BLUE_CB_RED_CR_LUT + red)) + (int32_t)(*(GREEN_CR_LUT + green)) + (int32_t)(*(BLUE_CR_LUT + blue)) + 128;
        
        (*(pOutAddr + JPEG_ConvertorParams.Y_MCU_LUT[offset]))  = (ycomp);
        (*(pOutAddr + JPEG_ConvertorParams.Cb_MCU_LUT[offset])) = (cbcomp);
        (*(pOutAddr + JPEG_ConvertorParams.Cr_MCU_LUT[offset])) = (crcomp);
        
        pInAddr += JPEG_BYTES_PER_PIXEL;
        offset++;
      }
      offset += (JPEG_ConvertorParams.H_factor - colones);
      refline += JPEG_ConvertorParams.ScaledWidth;
    }
    pOutAddr +=  JPEG_ConvertorParams.BlockSize;
  }

  return numberMCU;
}


/**
  * @brief  Retrive Encoding RGB to YCbCr color conversion function and block number  
  * @param  pJpegInfo  : JPEG_ConfTypeDef that contains the JPEG image informations.
  *                      These info are available in the HAL callback "HAL_JPEG_InfoReadyCallback".
  * @param  pFunction  : pointer to JPEG_RGBToYCbCr_Convert_Function , used to retrive the color conversion function 
  *                      depending of the jpeg image color space and chroma sampling info. 
  * @param ImageNbMCUs : pointer to uint32_t, used to retrive the total number of MCU blocks in the jpeg image.  
  * @retval HAL status : HAL_OK or HAL_ERROR.
  */
HAL_StatusTypeDef JPEG_GetEncodeColorConvertFunc(JPEG_ConfTypeDef *pJpegInfo, JPEG_RGBToYCbCr_Convert_Function *pFunction, uint32_t *ImageNbMCUs)
{
  uint32_t hMCU, vMCU;

  JPEG_ConvertorParams.ColorSpace = pJpegInfo->ColorSpace;
  JPEG_ConvertorParams.ChromaSubsampling = pJpegInfo->ChromaSubsampling;
  
  if(JPEG_ConvertorParams.ChromaSubsampling == JPEG_420_SUBSAMPLING)
  {
      *pFunction =  JPEG_ARGB_MCU_YCbCr420_ConvertBlocks;
  }
  else if (JPEG_ConvertorParams.ChromaSubsampling == JPEG_422_SUBSAMPLING)
  {
      *pFunction = JPEG_ARGB_MCU_YCbCr422_ConvertBlocks;
  }
  else if (JPEG_ConvertorParams.ChromaSubsampling == JPEG_444_SUBSAMPLING)
  {
      *pFunction = JPEG_ARGB_MCU_YCbCr444_ConvertBlocks;
  }
  else
  {
       return HAL_ERROR; /* Chroma SubSampling Not supported*/
  }

  JPEG_ConvertorParams.ImageWidth = pJpegInfo->ImageWidth;
  JPEG_ConvertorParams.ImageHeight = pJpegInfo->ImageHeight;
  JPEG_ConvertorParams.ImageSize_Bytes = pJpegInfo->ImageWidth * pJpegInfo->ImageHeight * JPEG_BYTES_PER_PIXEL;

  if((JPEG_ConvertorParams.ChromaSubsampling == JPEG_420_SUBSAMPLING) || (JPEG_ConvertorParams.ChromaSubsampling == JPEG_422_SUBSAMPLING))
  {
    JPEG_ConvertorParams.LineOffset = JPEG_ConvertorParams.ImageWidth % 16;
   
    JPEG_ConvertorParams.Y_MCU_LUT = JPEG_Y_MCU_LUT;
    
    if(JPEG_ConvertorParams.LineOffset != 0)
    {
      JPEG_ConvertorParams.LineOffset = 16 - JPEG_ConvertorParams.LineOffset;  
    }

    JPEG_ConvertorParams.H_factor = 16;
    
    if(JPEG_ConvertorParams.ChromaSubsampling == JPEG_420_SUBSAMPLING)
    {
      JPEG_ConvertorParams.V_factor  = 16;

      if(JPEG_ConvertorParams.ColorSpace == JPEG_YCBCR_COLORSPACE)
      {
        JPEG_ConvertorParams.BlockSize =  YCBCR_420_BLOCK_SIZE;
      }

      JPEG_ConvertorParams.Cb_MCU_LUT = JPEG_Cb_MCU_420_LUT;
      JPEG_ConvertorParams.Cr_MCU_LUT = JPEG_Cr_MCU_420_LUT;

      JPEG_ConvertorParams.K_MCU_LUT  = JPEG_K_MCU_420_LUT;
    }
    else /* 4:2:2*/
    {
      JPEG_ConvertorParams.V_factor = 8;

      if(JPEG_ConvertorParams.ColorSpace == JPEG_YCBCR_COLORSPACE)
      {
        JPEG_ConvertorParams.BlockSize =  YCBCR_422_BLOCK_SIZE;
      }

      JPEG_ConvertorParams.Cb_MCU_LUT = JPEG_Cb_MCU_422_LUT;
      JPEG_ConvertorParams.Cr_MCU_LUT = JPEG_Cr_MCU_422_LUT;

      JPEG_ConvertorParams.K_MCU_LUT  = JPEG_K_MCU_422_LUT;
    }
  }
  else if(JPEG_ConvertorParams.ChromaSubsampling == JPEG_444_SUBSAMPLING)
  {
    JPEG_ConvertorParams.LineOffset = JPEG_ConvertorParams.ImageWidth % 8;

    JPEG_ConvertorParams.Y_MCU_LUT = JPEG_Y_MCU_444_LUT;

    JPEG_ConvertorParams.Cb_MCU_LUT = JPEG_Cb_MCU_444_LUT;
    JPEG_ConvertorParams.Cr_MCU_LUT = JPEG_Cr_MCU_444_LUT;

    JPEG_ConvertorParams.K_MCU_LUT  = JPEG_K_MCU_444_LUT;

    if(JPEG_ConvertorParams.LineOffset != 0)
    {
      JPEG_ConvertorParams.LineOffset = 8 - JPEG_ConvertorParams.LineOffset;
    }
    JPEG_ConvertorParams.H_factor = 8;
    JPEG_ConvertorParams.V_factor = 8;

    if(JPEG_ConvertorParams.ColorSpace == JPEG_YCBCR_COLORSPACE)
    {
      JPEG_ConvertorParams.BlockSize = YCBCR_444_BLOCK_SIZE;
    }
  }
  else
  {
     return HAL_ERROR; /* Not supported*/
  }

  JPEG_ConvertorParams.WidthExtend = JPEG_ConvertorParams.ImageWidth + JPEG_ConvertorParams.LineOffset;
  JPEG_ConvertorParams.ScaledWidth = JPEG_BYTES_PER_PIXEL * JPEG_ConvertorParams.ImageWidth; 

  hMCU = (JPEG_ConvertorParams.ImageWidth / JPEG_ConvertorParams.H_factor);
  if((JPEG_ConvertorParams.ImageWidth % JPEG_ConvertorParams.H_factor) != 0)
  {
    hMCU++; /*+1 for horizenatl incomplete MCU */                
  }

  vMCU = (JPEG_ConvertorParams.ImageHeight / JPEG_ConvertorParams.V_factor);
  if((JPEG_ConvertorParams.ImageHeight % JPEG_ConvertorParams.V_factor) != 0)
  {
    vMCU++; /*+1 for vertical incomplete MCU */                
  }
  JPEG_ConvertorParams.MCU_Total_Nb = (hMCU * vMCU);
#ifdef JPEG_DEBUG
  debug_printf("hMCU = %d, vMCU = %d, total = %d\n", hMCU, vMCU, JPEG_ConvertorParams.MCU_Total_Nb);
#endif
  *ImageNbMCUs = JPEG_ConvertorParams.MCU_Total_Nb;
  
  return HAL_OK;
}

static uint8_t *jpeg_buffer;

void init_jpeg()
{
  JPEGDEVINFO *jpegEncoder = &JpegDeviceInfo;

#ifdef JPEG_DEBUG
  debug_printf("MCU_BUFSIZE = %d\n", MCU_BUFSIZE);
#endif

  jpegEncoder->encoder_flagId = osEventFlagsNew(&attributes_encoderFlag);
  jpegEncoder->hjpeg = &JPEG_Handle;
  jpegEncoder->sem_encoderId = osSemaphoreNew(1, 1, &attributes_encoder_sem);
  jpegEncoder->sem_jpegbufId = osSemaphoreNew(1, 1, &attributes_jpegbuf_sem);

  /* Init the HAL JPEG driver */
  JPEG_Handle.Instance = JPEG;
  HAL_JPEG_Init(&JPEG_Handle);

  jpeg_buffer = malloc(JPEG_OBSIZE);
}

void save_jpeg_start(int fnum, char *mode, int w, int h)
{
  int st;
  JPEGDEVINFO *jpegEncoder = &JpegDeviceInfo;

  osSemaphoreAcquire(jpegEncoder->sem_encoderId, osWaitForever);

  jpegEncoder->MCU_DataBuffer = (uint8_t *)MCU_Buffer;
  jpegEncoder->MCU_TotalNb = 0;
  jpegEncoder->Output_Is_Paused = jpegEncoder->Input_Is_Paused = 0;
  jpegEncoder->inbState = JPEG_BUFFER_EMPTY;

  /* Clear output buffer */
  jpegEncoder->outbState = JPEG_BUFFER_EMPTY;
  jpegEncoder->jpegOutAddress = jpeg_buffer;

  jpegEncoder->feed_unit = w * 2 * MAX_INPUT_LINES;

  /* Get RGB Info */
  jpegEncoder->JPEG_Info.ImageWidth = w;
  jpegEncoder->JPEG_Info.ImageHeight = h;

  /* Jpeg Encoding Setting to be set by users */
  jpegEncoder->JPEG_Info.ChromaSubsampling  = JPEG_444_SUBSAMPLING;
  jpegEncoder->JPEG_Info.ColorSpace         = JPEG_YCBCR_COLORSPACE;
  jpegEncoder->JPEG_Info.ImageQuality       = 75;

  JPEG_GetEncodeColorConvertFunc(&(jpegEncoder->JPEG_Info), &pRGBToYCbCr_Convert_Function, &(jpegEncoder->MCU_TotalNb));

  /* Setup JPEG encoder parameters */
  st = HAL_JPEG_ConfigEncoding(jpegEncoder->hjpeg, &(jpegEncoder->JPEG_Info));
  if (st != HAL_OK)
  {
    debug_printf("ConfigEncoding failed. (%x)\n", st);
  }
#ifdef DO_POST
  postShotRequest(SCREENSHOT_CREATE, mode, fnum);
#endif
}

void save_jpeg_data(uint8_t *colorp, int w, int h)
{
  uint32_t nb;
  int ch;
  int st;
  JPEGDEVINFO *jpegEncoder = &JpegDeviceInfo;
  uint8_t *inb;
  uint32_t flags = 0;

  inb = (uint8_t *)colorp;

  osSemaphoreAcquire(jpegEncoder->sem_jpegbufId, osWaitForever);

  for (ch = 0; ch < h; ch += MAX_INPUT_LINES)
  {

    if (ch == 0)
    {
      /* Convert RGB565 image to MCU blocks */
      pRGBToYCbCr_Convert_Function(inb, jpegEncoder->MCU_DataBuffer, 0, jpegEncoder->feed_unit, &nb);
#ifdef JPEG_DEBUG
      debug_printf("%d --> %d, %d\n", ch, nmcu, nb);
#endif
      inb += jpegEncoder->feed_unit;
      jpegEncoder->inbState = JPEG_BUFFER_FULL;

      /* This is the first MCU Block to encode. Start JPEG encoder hardware. */

      st = HAL_JPEG_Encode_DMA(jpegEncoder->hjpeg,
              jpegEncoder->MCU_DataBuffer, nb,
              jpegEncoder->jpegOutAddress, JPEG_OBSIZE);
      if (st != HAL_OK)
        debug_printf("Encode failed.\n");
    }

    do {
      flags = osEventFlagsWait(jpegEncoder->encoder_flagId, 0x0f, osFlagsWaitAny, osWaitForever);

      if (flags & ENCODER_DATAREQ)
      {
        if (jpegEncoder->Input_Is_Paused == 1)
        {
          /* Convert RGB565 image to MCU blocks */
          pRGBToYCbCr_Convert_Function(inb, jpegEncoder->MCU_DataBuffer, 0, jpegEncoder->feed_unit, &nb);
#ifdef JPEG_DEBUG
          debug_printf("%d --> %d, %d\n", ch, nmcu, nb);
#endif
          inb += jpegEncoder->feed_unit;
          jpegEncoder->inbState = JPEG_BUFFER_FULL;
          jpegEncoder->Input_Is_Paused = 0;

          SCB_CleanInvalidateDCache();
          st = HAL_JPEG_Resume(jpegEncoder->hjpeg, JPEG_PAUSE_RESUME_INPUT);
#ifdef JPEG_DEBUG
          debug_printf("Resume input (%d)\n", st);
#endif
        }
      }
      if (flags & ENCODER_DATAREADY)
      {
        if ((jpegEncoder->Output_Is_Paused == 1) && (jpegEncoder->outbState == JPEG_BUFFER_FULL))
        {
          jpegEncoder->jpegOutAddress = jpeg_buffer;
          jpegEncoder->outbState = JPEG_BUFFER_EMPTY;
          jpegEncoder->Output_Is_Paused = 0;
          st = HAL_JPEG_Resume(jpegEncoder->hjpeg, JPEG_PAUSE_RESUME_OUTPUT);
#ifdef JPEG_DEBUG
          debug_printf("Resume output: %d\n", st);
#endif
        }
      }
    } while ((flags & (ENCODER_DATAREQ|ENCODER_COMPLETE)) == 0);
  }
#ifdef JPEG_DEBUG
  debug_printf("feed done.\n");
#endif
  while ((flags & ENCODER_COMPLETE) == 0)
  {
    flags = osEventFlagsWait(jpegEncoder->encoder_flagId, 0x0f, osFlagsWaitAny, osWaitForever);
  }
#ifdef JPEG_DEBUG
  debug_printf("Conv fished\n");
#endif
}

void save_jpeg_finish()
{
  JPEGDEVINFO *jpegEncoder = &JpegDeviceInfo;

  osSemaphoreRelease(jpegEncoder->sem_jpegbufId);
}

/**
  * @brief  JPEG Data Ready callback
  * @param hjpeg: JPEG handle pointer
  * @param pDataOut: pointer to the output data buffer
  * @param OutDataLength: length of output buffer in bytes
  * @retval None
  */
void HAL_JPEG_DataReadyCallback (JPEG_HandleTypeDef *hjpeg, uint8_t *pDataOut, uint32_t OutDataLength)
{
  JPEGDEVINFO *jpegEncoder = &JpegDeviceInfo;
  int st;

  debug_printf("DataReady: %x @ %x\n", OutDataLength, pDataOut);
  jpegEncoder->outbState = JPEG_BUFFER_FULL;
  st = HAL_JPEG_Pause(jpegEncoder->hjpeg, JPEG_PAUSE_RESUME_OUTPUT);
  if (st == HAL_OK)
  {
    jpegEncoder->Output_Is_Paused = 1;
  }
  SCB_CleanInvalidateDCache();

#ifdef DO_POST
  postShotRequest(SCREENSHOT_WRITE, pDataOut, OutDataLength);
#endif
}

void save_jpeg_write_done()
{
  JPEGDEVINFO *jpegEncoder = &JpegDeviceInfo;

  osEventFlagsSet(jpegEncoder->encoder_flagId, ENCODER_DATAREADY);
}

/**
  * @brief JPEG Get Data callback
  * @param hjpeg: JPEG handle pointer
  * @param NbEncodedData: Number of encoded (consummed) bytes from input buffer
  * @retval None
  */
void HAL_JPEG_GetDataCallback(JPEG_HandleTypeDef *hjpeg, uint32_t NbEncodedData)
{
  JPEGDEVINFO *jpegEncoder = &JpegDeviceInfo;
  int st;

#ifdef JPEG_DEBUG
  debug_printf("%s:\n", __FUNCTION__);
#endif
  st = HAL_JPEG_Pause(hjpeg, JPEG_PAUSE_RESUME_INPUT);
  if (st == HAL_OK)
  {
    jpegEncoder->Input_Is_Paused = 1;
    jpegEncoder->inbState = JPEG_BUFFER_EMPTY;
  }
  osEventFlagsSet(jpegEncoder->encoder_flagId, ENCODER_DATAREQ);
}

/**
  * @brief  JPEG Error callback
  * @param hjpeg: JPEG handle pointer
  * @retval None
  */
void HAL_JPEG_ErrorCallback(JPEG_HandleTypeDef *hjpeg)
{
  debug_printf("%s:\n", __FUNCTION__);
  Error_Handler();
}

/*
  * @brief JPEG Decode complete callback
  * @param hjpeg: JPEG handle pointer
  * @retval None
  */
void HAL_JPEG_EncodeCpltCallback(JPEG_HandleTypeDef *hjpeg)
{
  JPEGDEVINFO *jpegEncoder = &JpegDeviceInfo;

  debug_printf("%s:\n", __FUNCTION__);
#ifdef DO_POST
  postShotRequest(SCREENSHOT_CLOSE, NULL, 0);
#endif
  osSemaphoreRelease(jpegEncoder->sem_encoderId);
  osEventFlagsSet(jpegEncoder->encoder_flagId, ENCODER_COMPLETE);
}
