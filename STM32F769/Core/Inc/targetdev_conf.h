#ifndef __TARGETDEV_CONF_H__
#define __TARGETDEV_CONF_H__

#include "stm32f769i_discovery_conf.h"
#include "stm32f769i_discovery.h"
#include "stm32f769i_discovery_lcd.h"
#include "stm32f769i_discovery_ts.h"
#include "stm32f769i_discovery_qspi.h"

#define	LCD_WIDTH	800
#define	LCD_HEIGHT	480
#define TFT_DRAW_HEIGHT 120

#define	DOOM_SCWIDTH	640
#define	DOOM_SCHEIGHT	400

#define	LTDC_HANDLE	hltdc_discovery
#define	DMA2D_HANDLE	hdma2d_discovery

#define	LV_DEMO_MUSIC_LARGE	0

#define QSPI_ADDR       0x90000000

/*
 * SDRAM Area: C0000000 - C0FFFFFF (16MB)
 *
 * C0000000 - C01FFFF0   LCD Frame buffer
 * C0200000 - C09FFFFF   RTOS Heap area
 * C0A00000 - C0FFFFFF   LVGL Heap area
 */
#define	RTOS_HEAP_ADDR	0xC0500000
#define	RTOS_HEAP_SIZE	0x00A00000
#define	LV_HEAP_ADDR	0xC0F00000
#define	LV_HEAP_SIZE	0x00100000

#define	SECTION_USBSRAM		SECTION_AHBSRAM __attribute__((aligned(4)))
#define	SECTION_AUDIOSRAM	SECTION_SDRAM
#define SECTION_AXISRAM 	SECTION_SDRAM
#define SECTION_AUDIOBUFF	SECTION_DTCMRAM
#define	SECTION_PREFFT		SECTION_SDRAM
#define	SECTION_FLACPOOL	SECTION_SDRAM

#define	ZOOM_BASE	(400)
#define	JOY_RAD		32
#define	JOY_DIA		64
#define	JOY_DIV		4

#endif
