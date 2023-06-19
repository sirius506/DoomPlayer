#include "DoomPlayer.h"
#include "lvgl.h"
#include "audio_output.h"
#include "stm32f769i_discovery_audio.h"
#include "board_if.h"
#include "app_task.h"
#include "app_gui.h"

extern LTDC_HandleTypeDef  LTDC_HANDLE;

static int board_lcd_mode;

typedef struct
{
    char *name;
    const uint8_t *data;
    unsigned int w;
    unsigned int h;
} txt_font_t;

#include "../chocolate-doom-3.0.1/textscreen/fonts/normal.h"

/**
 *  GUI layout parameters for 800x480 32F769DISCOVERY board
 */

const GUI_LAYOUT GuiLayout = {
  .font_title = &lv_font_montserrat_40,
  .font_small = &lv_font_montserrat_22,
  .font_large = &lv_font_montserrat_32,

  .spinner_width = 20,

  /* Menu screen */

  .mb_yoffset = 135,
  .mb_height = 95,
  .mb_olw = 8,

  .audio_box_width = 200,
  .audio_box_height = 145,
  .audio_box_yoffset = 120,
  .audio_button_height = 60,

  /* DualSense demo screen */

  .led_rad = 13,
  .joyl_x = 195,
  .joyr_x = 365,
  .joy_y = 320,
  .joy_divisor = 4,

  .line_width = 10,

  .bar_width = 20,
  .bar_height = 168,
  .bar_xpos = -84,
  .bar_ypos =  -105,

  .padi_xpos = 190,
  .padi_ypos = 80,
  .padi_width = 180,
  .padi_height = 95,
};

const lv_point_t ButtonPositions[DS_NUM_BUTTONS] = {
  { 405, 170 }, /* Square */
  { 440, 220 }, /* Cross */
  { 480, 170 }, /* Circle */
  { 440, 125 }, /* Triangle */
  { 115,  70 }, /* L1 */
  { 445,  70 }, /* R1 */
  { 100,  50 },	/* L2 */
  { 450,  50 },	/* R2 */
  { 155, 110 },	/* Create */
  { 405, 110 },	/* Option */
  { 195, 245 },	/* L3 */
  { 365, 245 },	/* R3 */
  { 275, 240 },	/* PS */
  { 275,  80 },	/* Touch */
  { 275, 270 },	/* MUTE */
  { 110, 132 },	/* 15 - Up */
  {  77, 170 }, /* 16 - Left */
  { 145, 170 }, /* 17 - Right */
  { 110, 200 }, /* 18 - Down */
};

#define ENDOOM_W 80
#define ENDOOM_H 25

static SECTION_DTCMRAM QSPI_Info QSPI_FlashInfo;

SECTION_AHBSRAM uint32_t saved_palette[256];
SECTION_FBRAM   uint8_t  saved_doom_screen[DOOM_SCWIDTH*DOOM_SCHEIGHT];
SECTION_FBRAM   uint16_t RGB_Doom_Screen[DOOM_SCWIDTH*DOOM_SCHEIGHT];
SECTION_FBRAM   uint16_t lvgl_alpha_screen[DOOM_SCWIDTH*DOOM_SCHEIGHT];

extern uint16_t lvgl_fb[];

static volatile uint8_t doom_dump_request;
static int gui_alpha;
static int lcd_brightness;

void Board_SDRAM_Init()
{
  BSP_SDRAM_Init();
  memset((void *)0xC0000000, 0, 1024*1024*16);
}

void Board_LCD_Init()
{
  BSP_LCD_Init();
  BSP_LCD_LayerDefaultInit(0, 0);
  BSP_LCD_LayerDefaultInit(1, 0);
  BSP_LCD_SelectLayer(1);
  BSP_LCD_SetBrightness(70);
  lcd_brightness = 70;

  BSP_PB_Init(BUTTON_WAKEUP, BUTTON_MODE_EXTI);
}

void Board_Set_Brightness(int val)
{
  BSP_LCD_SetBrightness(val);
  lcd_brightness = val;
}

int Board_Get_Brightness()
{
  return lcd_brightness;
}

void Board_GUI_LayerVisible(int alpha)
{
  BSP_LCD_SetTransparency(1, alpha);
  gui_alpha = alpha;
}

void Board_GUI_LayerInvisible()
{
  BSP_LCD_SetTransparency(1, 0);
  gui_alpha = 0;
}

void Board_DOOM_LayerInvisible()
{
  BSP_LCD_SetTransparency(0, 0);
  board_lcd_mode = LCD_MODE_LVGL;
}

void Board_Touch_Init(int width, int height)
{
  BSP_TS_Init(LCD_WIDTH, LCD_HEIGHT);
  BSP_TS_ITConfig();
}

void Board_Touch_GetState(lv_indev_data_t *tp)
{
  TS_StateTypeDef TS_State;

  BSP_TS_GetState(&TS_State);

  if (TS_State.touchDetected)
    tp->state = LV_INDEV_STATE_PRESSED;
  else
    tp->state = LV_INDEV_STATE_RELEASED;
  tp->point.x = TS_State.touchX[0];
  tp->point.y = TS_State.touchY[0];
}

void BSP_AUDIO_OUT_TransferComplete_CallBack(void)
{
  mix_request_data(1);
}

void BSP_AUDIO_OUT_HalfTransfer_CallBack(void)
{
  mix_request_data(0);
}

void Board_SetVolume(int vol)
{
   BSP_AUDIO_OUT_SetVolume(vol/2);
}

void Board_Audio_Init()
{
  if (BSP_AUDIO_OUT_Init(OUTPUT_DEVICE_HEADPHONE, 50, BSP_AUDIO_FREQUENCY_44K) != AUDIO_OK)
    debug_printf("AUDIO_OUT_Init failed.\n");
}

void Board_Audio_Output_Start(uint16_t *buffer, int size)
{
  BSP_AUDIO_OUT_Play(buffer, size);
}

void Board_Audio_Output_Stop()
{
  BSP_AUDIO_OUT_Stop(0);
}

void Board_DoomModeLCD()
{
  BSP_LCD_SetTransparency(1, 0);
  BSP_LCD_SetLayerVisible(0, ENABLE);
  board_lcd_mode = LCD_MODE_DOOM;
}

int Board_LCD_Mode()
{
  return board_lcd_mode;
}

/**
 * @brief Expand 320x200 Doom screen image to 640x400 pixels.
 */
void Board_ScreenExpand(uint8_t *sp, uint32_t *palette)
{
  int x, y;
  uint8_t *dp, *nextfb;

  nextfb = dp = GetNextFB();

  if (palette)
  {
    HAL_LTDC_ConfigCLUT(&LTDC_HANDLE, (uint32_t *)palette, 256, LTDC_LAYER_1);
    HAL_LTDC_EnableCLUT(&LTDC_HANDLE, LTDC_LAYER_1);
    memcpy(saved_palette, palette, sizeof(uint32_t) * 256);
  }

  for (y = 0; y < 200; y++)
  {
    /* Repeat same pixel data twice to double the image
     * in horizontal direction.
     */
    for (x = 0; x < 320; x += 4)
    {
      *dp++ = *sp;
      *dp++ = *sp++;
      *dp++ = *sp;
      *dp++ = *sp++;
      *dp++ = *sp;
      *dp++ = *sp++;
      *dp++ = *sp;
      *dp++ = *sp++;
    }

    dp += 640;		// Skip one raster
  }
  /* Use DMA2D to repate the same rater image in vertical direction. */

  DMA2D->CR = 0;
  DMA2D->FGPFCCR = 0x05;	// L8 format
  DMA2D->FGMAR = (uint32_t)nextfb;
  DMA2D->FGOR = DOOM_SCWIDTH;
  DMA2D->OMAR = (uint32_t)(nextfb + 640);
  DMA2D->OOR = DOOM_SCWIDTH;
  DMA2D->NLR = (DOOM_SCWIDTH << DMA2D_NLR_PL_Pos) | (DOOM_SCHEIGHT << DMA2D_NLR_NL_Pos);

  /*start transfer*/
  DMA2D->CR |= DMA2D_CR_START_Msk;

  BSP_SetNextFB(nextfb);

  if (doom_dump_request)
  {
    while (DMA2D->CR & DMA2D_CR_START)
      ;
    memcpy(saved_doom_screen, nextfb, sizeof(saved_doom_screen));
    postGuiEventMessage(GUIEV_DOOM_CAPTURE, 0, NULL, NULL);
    doom_dump_request = 0;
  }
}

void Request_Doom_Shnapshot()
{
  doom_dump_request = 1;
}

int Board_FlashInfo()
{
  /* Read the OSPI memory info */
  if(BSP_QSPI_GetInfo(&QSPI_FlashInfo) != QSPI_OK)
  {
    return -1;
  }

  BSP_QSPI_DeInit();

  BSP_QSPI_Init();

  BSP_QSPI_EnableMemoryMappedMode();
  return MX25L512_FLASH_SIZE;
}

void Board_Flash_Init()
{
  BSP_QSPI_DeInit();

  BSP_QSPI_Init();

  BSP_QSPI_EnableMemoryMappedMode();
}

void Board_Flash_ReInit(int mapmode)
{
  BSP_QSPI_DeInit();
  BSP_QSPI_Init();

  if (mapmode)
    BSP_QSPI_EnableMemoryMappedMode();
}

int Board_EraseSectorSize()
{
  int block_size;

  block_size = QSPI_FlashInfo.EraseSectorSize;
  return block_size;
}

void Board_Erase_Block(uint32_t baddr)
{
    BSP_QSPI_Erase_Block(baddr);
    while(BSP_QSPI_GetStatus() == QSPI_BUSY)
      osDelay(3);
}

int Board_Flash_Write(uint8_t *bp, uint32_t baddr, int len)
{
  int res;

  res = BSP_QSPI_Write(bp, baddr, len);
  return res;
}

void Board_AUDIO_OUT_HalfTransfer_CallBack()
{
  BSP_AUDIO_OUT_HalfTransfer_CallBack();
}

void Board_AUDIO_OUT_TransferComplete_CallBack()
{
  BSP_AUDIO_OUT_TransferComplete_CallBack();
}

static lv_img_dsc_t Endoom_Image;

/*
0 Black         #000000
1 Blue          #0000FF
2 Green         #008000		00000 100000 00000	0x0400
3 Cyan          #00FFFF         00000 111111 11111      0x07FF
4 Red           #FF0000		11111 000000 00000	0xF800
5 Magenta       #FF00FF		11111 000000 11111	0xF81F
6 Brown         #A52A2A		10100 001010 00101	0xA145
7 Light Gray    #D3D3D3		11010 110100 11010	0xD69A

8 Dark Gray     #A9A9A9		10101 101010 10101	0xAD55
9 Light Blue    #ADD8E6		10101 110110 11100	0xAEDC
A Light Green   #90EE90		10010 111011 10010	0x9772
B Ligh Cyan     #E0FFFF		11100 111111 11111	0xE7FF
C Light Red     #FFCCCB		11111 110011 11001	0xFE79
D Light Magenta #FF80FF		11111 100000 11111	0xFC1F
E Yellow        #FFFF00		11111 111111 00000	0xFFE0
F Bright White  #FFFFFF		
*/

static const uint8_t cindex[4*16] = {
 0x00, 0x00, 0x00, 0xff,
 0xFF, 0x00, 0x00, 0xff,
 0x00, 0x80, 0x00, 0xff,
 0xFF, 0xFF, 0x00, 0xff,
 0x00, 0x00, 0xFF, 0xff,
 0xFF, 0x00, 0xFF, 0xff,
 0x2A, 0x2A, 0xA5, 0xff,
 0xD3, 0xD3, 0xD3, 0xff,
 0xA9, 0xA9, 0xA9, 0xff,
 0xE6, 0xD8, 0xAD, 0xff,
 0x90, 0xEE, 0x90, 0xff,
 0xFF, 0xFF, 0xE0, 0xff,
 0xCB, 0xCC, 0xFF, 0xff,
 0xFF, 0x80, 0xFF, 0xff,
 0x00, 0xFF, 0xFF, 0xff,
 0x00, 0xFF, 0xFF, 0xff,
};

static void setGlyph(uint8_t *pwp, const uint32_t boffset, const txt_font_t *font, uint8_t attr)
{
  uint8_t *wp;
  const uint8_t *gbp;
  uint8_t maskBit;
  uint16_t fgColor, bgColor, color;
  int x, y;

  gbp = font->data + boffset / 8;
  maskBit = 1 << (boffset & 7);
  fgColor = attr & 0x0F;
  bgColor = (attr >> 4) & 0x07;

  for (y = 0; y < font->h; y++)
  {
    wp = pwp;
    for (x = 0; x < font->w; x++)
    {
      if (maskBit == 0)
      {
        gbp++;
        maskBit = 0x01;
      }
      color = (*gbp & maskBit)? fgColor : bgColor;
      if (x & 1)
      {
        *wp++ |= color;
      }
      else
      {
        *wp = color << 4;
      }
      maskBit <<= 1;
    }
    pwp += font->w * ENDOOM_W / 2;
  }
}

lv_img_dsc_t *
Board_Endoom(uint8_t *bp)
{
  const txt_font_t *font = &normal_font;
  lv_img_dsc_t *pimg;
  uint8_t cdata, attr;
  int fbsize;

  pimg = &Endoom_Image;
  pimg->header.always_zero = 0;
  pimg->header.w = font->w * ENDOOM_W;
  pimg->header.h = font->h * ENDOOM_H;
  pimg->header.cf = LV_IMG_CF_INDEXED_4BIT;
  pimg->data_size = pimg->header.w * pimg->header.h / 2 + 4 * 16;
  pimg->data = malloc(pimg->data_size);

  if (pimg->data == NULL)
    debug_printf("Failed to allocate bitmap space.\n");

  fbsize = font->w * font->h;		// glyph bitmap size in bits

  uint8_t *pdsty;
  uint8_t *pwp;
  int x, y;

  pwp = (uint8_t *)pimg->data;
  memcpy(pwp, cindex, 4 * 16);
  pdsty = (uint8_t *)(pimg->data + 4 * 16);
  for ( y = 0; y < ENDOOM_H; y++)
  {
    pwp = pdsty;
    for (x = 0; x < ENDOOM_W; x++)
    {
      cdata = *bp++;
      attr = *bp++ & 0x7F;
      setGlyph(pwp, cdata * fbsize, font, attr);
      pwp += font->w / 2;
    }
    pdsty += font->w * ENDOOM_W * font->h / 2;
  }
  SCB_CleanDCache();
  return pimg;
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  extern void app_screenshot();


  if (GPIO_Pin == LCD_INT_Pin)
    PostAppTouchEvent();

  if (GPIO_Pin == WAKEUP_BUTTON_PIN)
  {
    if (board_lcd_mode == LCD_MODE_DOOM)
      Request_Doom_Shnapshot();
    else
      app_screenshot();
  }
}

uint8_t *Board_DoomCapture()
{
  int i;
  uint16_t *wp;

  while (DMA2D->CR & DMA2D_CR_START)
    ;

  if (gui_alpha)	// LVGL layer is visible
  {
    /* Convert lvgl_fb contents to ARGB1555 format */
    SCB_CleanInvalidateDCache();
    DMA2D->CR = 0; 
    DMA2D->OOR = 0; 
    DMA2D->FGPFCCR = (gui_alpha << 24) | DMA2D_INPUT_RGB565;
    wp = lvgl_fb + (LCD_HEIGHT-DOOM_SCHEIGHT)/2*LCD_WIDTH + (LCD_WIDTH-DOOM_SCWIDTH)/2;
    DMA2D->FGMAR = (uint32_t) wp;
    DMA2D->FGOR = LCD_WIDTH-DOOM_SCWIDTH;
    DMA2D->OMAR = (uint32_t)lvgl_alpha_screen;
    DMA2D->OPFCCR = DMA2D_OUTPUT_ARGB1555;
    DMA2D->NLR = (DOOM_SCWIDTH << DMA2D_NLR_PL_Pos) | (DOOM_SCHEIGHT << DMA2D_NLR_NL_Pos);
  
    DMA2D->CR |= DMA2D_M2M_PFC | DMA2D_CR_START;
    while (DMA2D->CR & DMA2D_CR_START)
      ;
    
    /* Make green pixels to transparent */
    for (wp = lvgl_alpha_screen; wp < &lvgl_alpha_screen[DOOM_SCWIDTH*DOOM_SCHEIGHT]; wp++)
    {
      /* 1000 0011 1110 0000 */
      if (*wp == 0x83E0) 
        *wp = 0x03E0;
    }
    SCB_CleanInvalidateDCache();
    /* Load palette */
    DMA2D->CR = 0;
    for (i = 0; i < 256; i++)
      DMA2D->BGCLUT[i] = saved_palette[i];

    /* Blend Doom screen and lvgl screen */
    DMA2D->CR = 0;
    DMA2D->FGOR = 0;
    DMA2D->BGOR = 0;
    DMA2D->FGPFCCR = DMA2D_INPUT_ARGB1555;
    DMA2D->FGMAR = (uint32_t)lvgl_alpha_screen;
    DMA2D->BGPFCCR = (255 << 8) | DMA2D_INPUT_L8;
    DMA2D->BGMAR = (uint32_t)saved_doom_screen;
    DMA2D->OMAR = (uint32_t)RGB_Doom_Screen;
    DMA2D->OPFCCR = DMA2D_OUTPUT_RGB565;
    DMA2D->NLR = (DOOM_SCWIDTH << DMA2D_NLR_PL_Pos) | (DOOM_SCHEIGHT << DMA2D_NLR_NL_Pos);

    DMA2D->CR |= DMA2D_M2M_BLEND | DMA2D_CR_START;
    while (DMA2D->CR & DMA2D_CR_START)
      ;
  }
  else
  {
    DMA2D->CR = 0;
    for (i = 0; i < 256; i++)
      DMA2D->FGCLUT[i] = saved_palette[i];
  
    SCB_CleanInvalidateDCache();
    /* Convert L8 format image to RGB565 format */
    DMA2D->CR = 0;
    DMA2D->FGOR = 0; 
    DMA2D->OOR = 0; 
    DMA2D->FGPFCCR = (255 << 8) | DMA2D_INPUT_L8;
    DMA2D->FGMAR = (uint32_t)saved_doom_screen;
    DMA2D->OMAR = (uint32_t)RGB_Doom_Screen;
    DMA2D->OPFCCR = DMA2D_OUTPUT_RGB565;
    DMA2D->NLR = (DOOM_SCWIDTH << DMA2D_NLR_PL_Pos) | (DOOM_SCHEIGHT << DMA2D_NLR_NL_Pos);
  
    DMA2D->CR |= DMA2D_M2M_PFC | DMA2D_CR_START;
    while (DMA2D->CR & DMA2D_CR_START)
      ;
  }
  SCB_CleanInvalidateDCache();
  return (uint8_t *)RGB_Doom_Screen;
}
