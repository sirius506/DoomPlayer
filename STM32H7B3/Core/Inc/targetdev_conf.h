#ifndef __TARGETDEV_CONF_H__
#define __TARGETDEV_CONF_H__

#include "stm32h7b3i_discovery_conf.h"
#include "stm32h7b3i_discovery.h"
#include "stm32h7b3i_discovery_lcd.h"
#include "stm32h7b3i_discovery_ts.h"
#include "stm32h7b3i_discovery_audio.h"
#include "stm32h7b3i_discovery_ospi.h"

#define	LCD_WIDTH	480
#define	LCD_HEIGHT	272
#define TFT_DRAW_HEIGHT 68

#define	DOOM_SCWIDTH	LCD_WIDTH
#define	DOOM_SCHEIGHT	LCD_HEIGHT

#define	LTDC_HANDLE	hlcd_ltdc
#define	DMA2D_HANDLE	hlcd_dma2d

#define QSPI_ADDR       0x90000000

/*
 * SDRAM Area: D0000000 - D0FFFFFF (16MB)
 *
 * D0000000 - D00BB800   LCD Frame buffer
 * D0200000 - D021FFFF   LVGL Heap area
 * D0400000 - D0FFFFFF   RTOS Heap area
 */
#define	LCD_FB_ADDR	0xD0000000
#define	LCD_FB_SIZE	(480*272*2)
//#define LV_HEAP_ON_SDRAM
#ifdef LV_HEAP_ON_SDRAM
#define	LV_HEAP_ADDR	0xD0200000
#define	LV_HEAP_SIZE	(1024*512)
#else
#define	LV_HEAP_ADDR	0x240E0000
#define	LV_HEAP_SIZE	(1024*128)
#endif
#define	RTOS_HEAP_ADDR	0xD0400000
#define	RTOS_HEAP_SIZE	(1024*1024*12)

#define	SECTION_USBSRAM		SECTION_AHBSRAM
#define	SECTION_AUDIOSRAM	SECTION_AHBSRAM
#define	SECTION_AUDIOBUFF	SECTION_AHBSRAM
#define SECTION_PREFFT          SECTION_AXISRAM
#define SECTION_FLACPOOL        SECTION_AXISRAM

#define	ZOOM_BASE	(LV_IMG_ZOOM_NONE)
#define	JOY_RAD		25
#define	JOY_DIA		51
#define	JOY_DIV		5

#endif
