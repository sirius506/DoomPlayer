#include "DoomPlayer.h"
#include "lvgl.h"
#include "audio_output.h"
#include "stm32h7b3i_discovery_sdram.h"
#include "board_if.h"
#include "app_task.h"
#include "app_gui.h"

extern LTDC_HandleTypeDef LTDC_HANDLE;
extern DMA2D_HandleTypeDef DMA2D_HANDLE;

static int board_lcd_mode;

const GUI_LAYOUT GuiLayout = {
  .font_title = &lv_font_montserrat_20,
  .font_small = &lv_font_montserrat_12,
  .font_large = &lv_font_montserrat_16,

  .spinner_width = 12,

  /* Menu screen */

  .mb_yoffset = 80,
  .mb_height = 50,
  .mb_olw = 4,

  .audio_box_width = 130,
  .audio_box_height = 96,
  .audio_box_yoffset = 70,
  .audio_button_height = 35,

  /* DualSense demo screen */

  .led_rad = 5,
  .joyl_x = 128,
  .joyr_x = 237,
  .joy_y = 205,
  .joy_divisor = 5,

  .line_width = 5,

  .bar_width = 10,
  .bar_height = 100,
  .bar_xpos = -50,
  .bar_ypos =  -85,

  .padi_xpos = 125,
  .padi_ypos = 35,
  .padi_width = 110,
  .padi_height = 60,
};

const lv_point_t ButtonPositions[DS_NUM_BUTTONS] = {
  { 261,  90 }, /* Square */
  { 284, 115 }, /* Cross */
  { 310,  89 }, /* Circle */
  { 284,  65 }, /* Triangle */
  {  80,  34 }, /* L1 */
  { 287,  34 }, /* R1 */
  {  53,  24 },	/* L2 */
  { 297,  24 },	/* R2 */
  { 104,  54 },	/* Create */
  { 261,  54 },	/* Option */
  { 126, 138 },	/* L3 */
  { 237, 138 },	/* R3 */
  { 182, 137 },	/* PS */
  { 182,  25 },	/* Touch */
  { 182, 157 },	/* MUTE */
  {  75,  70 },	/* 15 - Up */
  {  57,  90 }, /* 16 - Left */
  {  94,  90 }, /* 17 - Right */
  {  75, 109 }, /* 18 - Down */
};

typedef struct
{
    char *name;
    const uint8_t *data;
    unsigned int w;
    unsigned int h;
} txt_font_t;

#include "../chocolate-doom-3.0.1/textscreen/fonts/tftfont.h"

#define ENDOOM_W 80
#define ENDOOM_H 25

static BSP_AUDIO_Init_t AudioPlayInit;

static BSP_OSPI_NOR_Info_t OSPI_FlashInfo;
static BSP_OSPI_NOR_Init_t Flash;

SECTION_AHBSRAM uint32_t saved_palette[256];
SECTION_FBRAM   uint8_t  saved_doom_screen[LCD_WIDTH*LCD_HEIGHT];
SECTION_FBRAM   uint16_t RGB_Doom_Screen[LCD_WIDTH*LCD_HEIGHT];

static volatile uint8_t doom_dump_request;

void Board_SDRAM_Init()
{
  BSP_SDRAM_Init(0);
}

void Board_LCD_Init()
{
  /* Turn off LED1 and LED2 */
  HAL_GPIO_WritePin(USER_LED1_GPIO_Port, USER_LED1_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(USER_LED2_GPIO_Port, USER_LED2_Pin, GPIO_PIN_SET);

  BSP_LCD_Init(0, LCD_ORIENTATION_LANDSCAPE);
  BSP_LCD_SetLayerVisible(0, 0, DISABLE);		// Disable DOOM layer
  BSP_LCD_SetLayerVisible(0, 1, ENABLE);		// Enable LVGL layer
  BSP_LCD_SetBrightness(0, 70);
  board_lcd_mode = LCD_MODE_LVGL;
}

void Board_GUI_LayerVisible(int alpha)
{
  BSP_LCD_SetTransparency(0, 1, alpha);
}

void Board_GUI_LayerInvisible()
{
  BSP_LCD_SetTransparency(0, 1, 0);
}

void Board_DOOM_LayerInvisible()
{
  BSP_LCD_SetTransparency(0, 0, 0);
  board_lcd_mode = LCD_MODE_LVGL;
}

void Board_Touch_Init(int width, int height)
{
  TS_Init_t TS_Init;

  TS_Init.Width = width;
  TS_Init.Height = height;
  TS_Init.Orientation = TS_SWAP_XY;
  TS_Init.Accuracy = 0;
  BSP_TS_Init(0, &TS_Init);

  BSP_TS_EnableIT(0);
}

void Board_Touch_GetState(lv_indev_data_t *tp)
{
  TS_State_t TS_State;

  BSP_TS_GetState(0, &TS_State);

  if (TS_State.TouchDetected)
    tp->state = LV_INDEV_STATE_PRESSED;
  else
    tp->state = LV_INDEV_STATE_RELEASED;
  tp->point.x = TS_State.TouchX;
  tp->point.y = TS_State.TouchY;
}

void BSP_TS_Callback(uint32_t Instance)
{
  extern void PostAppTouchEvent();

  PostAppTouchEvent();
}

void BSP_AUDIO_OUT_TransferComplete_CallBack(uint32_t Instance)
{
  MIXCONTROL_EVENT evcode;

  /* Prevent unused argument(s) compilation warning */
  UNUSED(Instance);

  evcode.event = MIX_FILL_FULL;
  osMessageQueuePut(MixInfo.mixevqId, &evcode, 0, 0);
}

void BSP_AUDIO_OUT_HalfTransfer_CallBack(uint32_t Instance)
{
  MIXCONTROL_EVENT evcode;

  /* Prevent unused argument(s) compilation warning */
  UNUSED(Instance);

  evcode.event = MIX_FILL_HALF;
  osMessageQueuePut(MixInfo.mixevqId, &evcode, 0, 0);
}

void Board_SetVolume(int vol)
{
   BSP_AUDIO_OUT_SetVolume(0, vol);
}

void Board_Audio_Init()
{
  AudioPlayInit.Device = AUDIO_OUT_DEVICE_HEADPHONE;
  AudioPlayInit.ChannelsNbr = 2;
  AudioPlayInit.SampleRate = AUDIO_FREQUENCY_44K;
  AudioPlayInit.BitsPerSample = AUDIO_RESOLUTION_16B;
  AudioPlayInit.Volume = 75;

  if (BSP_AUDIO_OUT_Init(0, &AudioPlayInit))
    debug_printf("AUDIO_OUT_Init failed.\n");
}

void Board_Audio_Output_Start(uint16_t *buffer, int size)
{
  BSP_AUDIO_OUT_Play(0, (uint8_t *)buffer, size);
}

void Board_Audio_Output_Stop()
{
  BSP_AUDIO_OUT_Stop(0);
}

void Board_DoomModeLCD()
{
#if 0
  BSP_DoomModeLCD();
#endif
  BSP_LCD_SetTransparency(0, 1, 0);
  BSP_LCD_SetLayerVisible(0, 0, ENABLE);		// Enable Layer 0 -- Doom screen
  board_lcd_mode = LCD_MODE_DOOM;
}

int Board_LCD_Mode()
{
  return board_lcd_mode;
}

/*
 * @brief Expand 320x200 Doom screen to 480x320 pixels
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
    for (x = 0; x < 320; x += 4)
    {
      *dp++ = *sp;
      *dp++ = *sp++;
      *dp++ = *sp++;

      *dp++ = *sp;
      *dp++ = *sp++;
      *dp++ = *sp++;
    }
    if ((y < 8) || (y %3) == 0)
    {
      memcpy(dp, dp - 480, 480);
      dp += 480;
    }
  }
  BSP_SetNextFB(nextfb);

  if (doom_dump_request)
  {
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
  OSPI_FlashInfo.FlashSize          = (uint32_t)0x00;
  OSPI_FlashInfo.EraseSectorSize    = (uint32_t)0x00;
  OSPI_FlashInfo.EraseSectorsNumber = (uint32_t)0x00;
  OSPI_FlashInfo.ProgPageSize       = (uint32_t)0x00;
  OSPI_FlashInfo.ProgPagesNumber    = (uint32_t)0x00;

  /* Read the OSPI memory info */
  if(BSP_OSPI_NOR_GetInfo(0, &OSPI_FlashInfo) != BSP_ERROR_NONE)
    return -1;
  return OSPI_FlashInfo.FlashSize;
}

void Board_Flash_Init()
{
  BSP_OSPI_NOR_DeInit(0);

  /* OSPI device configuration */
  Flash.InterfaceMode = BSP_OSPI_NOR_OPI_MODE;		// Octo SPI mode
  Flash.TransferRate  = BSP_OSPI_NOR_DTR_TRANSFER;	// Double Transfer rate
  BSP_OSPI_NOR_Init(0, &Flash);

  BSP_OSPI_NOR_EnableMemoryMappedMode(0);
}

void Board_Flash_ReInit(int mapmode)
{
  BSP_OSPI_NOR_DeInit(0);
  Flash.InterfaceMode = BSP_OSPI_NOR_OPI_MODE;
  Flash.TransferRate  = BSP_OSPI_NOR_STR_TRANSFER;
  BSP_OSPI_NOR_Init(0, &Flash);
  if (mapmode)
    BSP_OSPI_NOR_EnableMemoryMappedMode(0);
}


int Board_EraseSectorSize()
{
  int block_size;

  block_size = OSPI_FlashInfo.EraseSectorSize;
  return block_size;
}

void Board_Erase_Block(uint32_t baddr)
{
  BSP_OSPI_NOR_Erase_Block(0, baddr, MX25LM51245G_ERASE_64K);
  while(BSP_OSPI_NOR_GetStatus(0) == BSP_ERROR_BUSY)
    osDelay(3);
}

int Board_Flash_Write(uint8_t *bp, uint32_t baddr, int len)
{
  int res;

  res = BSP_OSPI_NOR_Write(0, bp, baddr, len);
  return res;
}

void Board_AUDIO_OUT_HalfTransfer_CallBack()
{
  BSP_AUDIO_OUT_HalfTransfer_CallBack(0);
}

void Board_AUDIO_OUT_TransferComplete_CallBack()
{
  BSP_AUDIO_OUT_TransferComplete_CallBack(0);
}

static lv_img_dsc_t Endoom_Image;

static const uint8_t cindex[4*16] = {
// Blue Green Red   Alpha
  0x00, 0x00, 0x00, 0xff,	// 0: Black         #000000
  0xFF, 0x00, 0x00, 0xff,	// 1: Blue          #0000FF
  0x00, 0x80, 0x00, 0xff,	// 2: Green         #008000
  0xFF, 0xFF, 0x00, 0xff,	// 3: Cyan          #00FFFF
  0x00, 0x00, 0xFF, 0xff,	// 4: Red           #FF0000
  0xFF, 0x00, 0xFF, 0xff,	// 5: Magenta       #FF00FF
  0x2A, 0x2A, 0xA5, 0xff,	// 6: Brown         #A52A2A
  0xD3, 0xD3, 0xD3, 0xff,	// 7: Light Gray    #D3D3D3
  0xA9, 0xA9, 0xA9, 0xff,	// 8: Dark Gray     #A9A9A9
  0xE6, 0xD8, 0xAD, 0xff,	// 9: Light Blue    #ADD8E6
  0x90, 0xEE, 0x90, 0xff,	// A: Light Green   #90EE90
  0xFF, 0xFF, 0xE0, 0xff,	// B: Ligh Cyan     #E0FFFF
  0xCB, 0xCC, 0xFF, 0xff,	// C: C Light Red   #FFCCCB
  0xFF, 0x80, 0xFF, 0xff,	// D: Light Magenta #FF80FF
  0x00, 0xFF, 0xFF, 0xff,	// E: Yellow        #FFFF00
  0x00, 0xFF, 0xFF, 0xff,	// F: Bright White  #FFFFFF		
};

static void setGlyph(uint8_t *pwp, const uint8_t *gp, const txt_font_t *font, uint8_t attr)
{
  uint8_t *wp;
  uint8_t maskBit;
  uint16_t fgColor, bgColor, color;
  int x, y;

  maskBit = 0x01;
  fgColor = attr & 0x0F;
  bgColor = (attr >> 4) & 0x07;

  for (y = 0; y < font->h; y++)
  {
    wp = pwp;
    for (x = 0; x < font->w; x++)
    {
      if (maskBit == 0)
      {
        gp++;
        maskBit = 0x01;
      }
      color = (*gp & maskBit)? fgColor : bgColor;
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
  const txt_font_t *font = &tft_font;
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

  fbsize = font->w * font->h / 8;		// glyph bitmap size

  uint8_t *pdsty;
  uint8_t *pwp;
  const uint8_t *gp;
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
      gp = font->data + cdata * fbsize;
      setGlyph(pwp, gp, font, attr);
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

  if (GPIO_Pin == WAKEUP_Pin)
  {
    if (board_lcd_mode == LCD_MODE_DOOM)
      Request_Doom_Shnapshot();
    else
      app_screenshot();
  }
}

uint8_t *Board_DoomCapture()
{
  DMA2D_HandleTypeDef *hdma2d = &DMA2D_HANDLE;
  int i;

  while (DMA2D->CR & DMA2D_CR_START)
    ;

  DMA2D->CR = 0;
  for (i = 0; i < 256; i++)
    DMA2D->FGCLUT[i] = saved_palette[i];

  SCB_CleanInvalidateDCache();
  /* Convert L8 format image to RGB565 format */
  hdma2d->Instance->CR = 0;
  hdma2d->Instance->FGOR = 0;
  hdma2d->Instance->OOR = 0;
  hdma2d->Instance->FGPFCCR = (255 << 8) | DMA2D_INPUT_L8;
  hdma2d->Instance->FGMAR = (uint32_t)saved_doom_screen;
  hdma2d->Instance->OMAR = (uint32_t)RGB_Doom_Screen;
  hdma2d->Instance->OPFCCR = DMA2D_OUTPUT_RGB565;
  hdma2d->Instance->NLR = (LCD_WIDTH << DMA2D_NLR_PL_Pos) | (LCD_HEIGHT << DMA2D_NLR_NL_Pos);

  hdma2d->Instance->CR |= DMA2D_M2M_PFC | DMA2D_CR_START;
  while (DMA2D->CR & DMA2D_CR_START)
    ;
  SCB_CleanInvalidateDCache();
  return (uint8_t *)RGB_Doom_Screen;
}
