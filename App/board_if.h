#ifndef _TARGET_DEV_H
#define _TARGET_DEV_H
#include "lvgl.h"

#define	LCD_MODE_LVGL	0
#define	LCD_MODE_DOOM	1

extern void Board_SDRAM_Init();
extern void Board_LCD_Init();
extern void Board_Touch_Init(int width, int height);
extern void Board_Touch_GetState(lv_indev_data_t *tp);
extern void Board_Audio_Init();
extern void Board_Audio_Output_Start(uint16_t *buffer, int size);
extern void Board_Audio_Output_Stop(void);
extern void Board_ScreenExpand(uint8_t *sp, uint32_t *palette);
extern void Board_SetVolume(int vol);
extern int Board_FlashInfo();
extern void Board_Flash_Init();
extern void Board_Flash_ReInit(int mapmode);
extern int Board_EraseSectorSize();
extern void Board_Erase_Block(uint32_t baddr);
extern int Board_Flash_Write(uint8_t *bp, uint32_t baddr, int len);
extern void Board_Audio_Init();
extern int  Board_LCD_Mode();
extern void Board_Audio_Output_Start(uint16_t *buffer, int size);
extern void Board_AUDIO_OUT_HalfTransfer_CallBack();
extern void Board_AUDIO_OUT_TransferComplete_CallBack();
extern void Board_GUI_LayerVisible(int alpha);
extern void Board_GUI_LayerInvisible();
extern void Board_DOOM_LayerInvisible();
extern void Board_DoomModeLCD();
extern lv_img_dsc_t *Board_Endoom(uint8_t *bp);
extern uint8_t *Board_DoomCapture();
extern int Board_Get_Brightness();
extern void Board_Set_Brightness(int val);

#endif
